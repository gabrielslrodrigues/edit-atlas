if(NOT DEFINED EDIT_ATLAS_EXECUTABLE)
    message(FATAL_ERROR "EDIT_ATLAS_EXECUTABLE is required.")
endif()

if(NOT EXISTS "${EDIT_ATLAS_EXECUTABLE}")
    message(
        FATAL_ERROR
        "Edit Atlas executable does not exist: ${EDIT_ATLAS_EXECUTABLE}"
    )
endif()

if(WIN32)
    set(edit_atlas_dependency_command dumpbin /dependents)
    set(edit_atlas_qt_widgets_pattern "Qt6Widgets(d)?\\.dll")
elseif(APPLE)
    set(edit_atlas_dependency_command otool -L)
    set(
        edit_atlas_qt_widgets_pattern
        "(libQt6Widgets[^ ]*\\.dylib|QtWidgets\\.framework)"
    )
elseif(UNIX)
    set(edit_atlas_dependency_command readelf -d)
    set(edit_atlas_qt_widgets_pattern "libQt6Widgets\\.so")
else()
    message(FATAL_ERROR "Dynamic Qt verification is unsupported on this host.")
endif()

execute_process(
    COMMAND
        ${edit_atlas_dependency_command}
        "${EDIT_ATLAS_EXECUTABLE}"
    RESULT_VARIABLE edit_atlas_dependency_result
    OUTPUT_VARIABLE edit_atlas_dependencies
    ERROR_VARIABLE edit_atlas_dependency_error
)

if(NOT edit_atlas_dependency_result EQUAL 0)
    message(
        FATAL_ERROR
        "Failed to inspect ${EDIT_ATLAS_EXECUTABLE}:\n"
        "${edit_atlas_dependency_error}"
    )
endif()

if(NOT edit_atlas_dependencies MATCHES "${edit_atlas_qt_widgets_pattern}")
    message(
        FATAL_ERROR
        "The executable does not dynamically depend on Qt Widgets:\n"
        "${edit_atlas_dependencies}"
    )
endif()

message(STATUS "Verified dynamic Qt Widgets linkage: ${EDIT_ATLAS_EXECUTABLE}")
