if(NOT DEFINED EDIT_ATLAS_DEPLOYMENT_ROOT)
    message(FATAL_ERROR "EDIT_ATLAS_DEPLOYMENT_ROOT is required.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/VerifyDynamicQtBinary.cmake")

function(edit_atlas_require_deployed_file description)
    set(edit_atlas_deployed_files)
    foreach(edit_atlas_deployed_pattern IN LISTS ARGN)
        file(
            GLOB_RECURSE edit_atlas_pattern_matches
            LIST_DIRECTORIES FALSE
            "${EDIT_ATLAS_DEPLOYMENT_ROOT}/${edit_atlas_deployed_pattern}"
        )
        list(APPEND edit_atlas_deployed_files ${edit_atlas_pattern_matches})
    endforeach()

    if(NOT edit_atlas_deployed_files)
        message(
            FATAL_ERROR
            "The staged application is missing ${description} under "
            "${EDIT_ATLAS_DEPLOYMENT_ROOT}."
        )
    endif()
endfunction()

edit_atlas_require_deployed_file(
    "Qt Widgets"
    "*Qt6Widgets*.dll"
    "*libQt6Widgets*.dylib"
    "*libQt6Widgets.so*"
    "*QtWidgets"
)

if(WIN32)
    set(edit_atlas_qt_concurrent_pattern "Qt6Concurrent(d)?\\.dll")
elseif(APPLE)
    set(
        edit_atlas_qt_concurrent_pattern
        "(libQt6Concurrent[^ ]*\\.dylib|QtConcurrent\\.framework)"
    )
elseif(LINUX)
    set(edit_atlas_qt_concurrent_pattern "libQt6Concurrent\\.so")
endif()

if(
    DEFINED edit_atlas_qt_concurrent_pattern
    AND edit_atlas_dependencies MATCHES
        "${edit_atlas_qt_concurrent_pattern}"
)
    edit_atlas_require_deployed_file(
        "Qt Concurrent"
        "*Qt6Concurrent*.dll"
        "*libQt6Concurrent*.dylib"
        "*libQt6Concurrent.so*"
        "*QtConcurrent"
    )
endif()

if(WIN32)
    edit_atlas_require_deployed_file(
        "the Windows Qt platform plugin"
        "*qwindows.dll"
    )
elseif(APPLE)
    edit_atlas_require_deployed_file(
        "the Cocoa Qt platform plugin"
        "*libqcocoa.dylib"
    )
elseif(UNIX)
    edit_atlas_require_deployed_file(
        "the XCB Qt platform plugin"
        "*libqxcb.so"
    )
    edit_atlas_require_deployed_file(
        "a Wayland Qt platform plugin"
        "*libqwayland*.so"
    )
endif()

edit_atlas_require_deployed_file(
    "the Edit Atlas third-party notices"
    "*THIRD_PARTY_NOTICES.md"
)
edit_atlas_require_deployed_file(
    "the Qt Base license notices"
    "*qtbase-copyright"
)

message(STATUS "Verified staged Qt runtime and compliance materials.")
