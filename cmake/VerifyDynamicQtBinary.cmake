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
    find_program(
        edit_atlas_dependency_tool
        NAMES dumpbin
        REQUIRED
    )
    set(edit_atlas_dependency_command
        "${edit_atlas_dependency_tool}"
        /dependents
    )
    set(edit_atlas_qt_widgets_pattern "Qt6Widgets(d)?\\.dll")
    set(edit_atlas_qt_qml_pattern "Qt6Qml(d)?\\.dll")
    set(edit_atlas_qt_quick_pattern "Qt6Quick(d)?\\.dll")
elseif(APPLE)
    find_program(
        edit_atlas_dependency_tool
        NAMES otool
        REQUIRED
    )
    set(edit_atlas_dependency_command "${edit_atlas_dependency_tool}" -L)
    set(
        edit_atlas_qt_widgets_pattern
        "(libQt6Widgets[^ ]*\\.dylib|QtWidgets\\.framework)"
    )
    set(
        edit_atlas_qt_qml_pattern
        "(libQt6Qml[^ ]*\\.dylib|QtQml\\.framework)"
    )
    set(
        edit_atlas_qt_quick_pattern
        "(libQt6Quick[^ ]*\\.dylib|QtQuick\\.framework)"
    )
elseif(LINUX)
    find_program(
        edit_atlas_dependency_tool
        NAMES readelf
        REQUIRED
    )
    set(edit_atlas_dependency_command "${edit_atlas_dependency_tool}" -d)
    set(edit_atlas_qt_widgets_pattern "libQt6Widgets\\.so")
    set(edit_atlas_qt_qml_pattern "libQt6Qml\\.so")
    set(edit_atlas_qt_quick_pattern "libQt6Quick\\.so")
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

if(edit_atlas_dependencies MATCHES "${edit_atlas_qt_widgets_pattern}")
    set(edit_atlas_detected_frontend "widgets")
elseif(
    edit_atlas_dependencies MATCHES "${edit_atlas_qt_qml_pattern}"
    AND edit_atlas_dependencies MATCHES "${edit_atlas_qt_quick_pattern}"
)
    set(edit_atlas_detected_frontend "quick")
else()
    message(
        FATAL_ERROR
        "The executable does not dynamically depend on the Qt Widgets or "
        "Qt Quick frontend runtime:\n"
        "${edit_atlas_dependencies}"
    )
endif()

if(
    DEFINED EDIT_ATLAS_EXPECTED_FRONTEND
    AND NOT "${EDIT_ATLAS_EXPECTED_FRONTEND}" STREQUAL
        "${edit_atlas_detected_frontend}"
)
    message(
        FATAL_ERROR
        "Expected the ${EDIT_ATLAS_EXPECTED_FRONTEND} frontend, but detected "
        "${edit_atlas_detected_frontend}: ${EDIT_ATLAS_EXECUTABLE}"
    )
endif()

message(
    STATUS
    "Verified dynamic Qt ${edit_atlas_detected_frontend} frontend linkage: "
    "${EDIT_ATLAS_EXECUTABLE}"
)
