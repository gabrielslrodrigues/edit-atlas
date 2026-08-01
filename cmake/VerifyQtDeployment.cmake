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

    set(
        edit_atlas_third_party_runtime_descriptions
        spdlog
        fmt
        libxlsxwriter
        minizip
    )
    set(
        edit_atlas_third_party_runtime_patterns
        "*spdlog*.dll"
        "*fmt*.dll"
        "*xlsxwriter*.dll"
        "*minizip*.dll"
    )
elseif(APPLE)
    edit_atlas_require_deployed_file(
        "the Cocoa Qt platform plugin"
        "*libqcocoa.dylib"
    )

    set(
        edit_atlas_third_party_runtime_descriptions
        spdlog
        fmt
        libxlsxwriter
        minizip
    )
    set(
        edit_atlas_third_party_runtime_patterns
        "*libspdlog*.dylib"
        "*libfmt*.dylib"
        "*libxlsxwriter*.dylib"
        "*libminizip*.dylib"
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

    set(
        edit_atlas_third_party_runtime_descriptions
        spdlog
        fmt
        libxlsxwriter
        minizip
    )
    set(
        edit_atlas_third_party_runtime_patterns
        "*libspdlog.so*"
        "*libfmt.so*"
        "*libxlsxwriter.so*"
        "*libminizip.so*"
    )
endif()

foreach(
    edit_atlas_third_party_runtime_description
    edit_atlas_third_party_runtime_pattern
    IN ZIP_LISTS
        edit_atlas_third_party_runtime_descriptions
        edit_atlas_third_party_runtime_patterns
)
    edit_atlas_require_deployed_file(
        "the ${edit_atlas_third_party_runtime_description} runtime library"
        "${edit_atlas_third_party_runtime_pattern}"
    )
endforeach()

if(WIN32)
    get_filename_component(
        edit_atlas_executable_directory
        "${EDIT_ATLAS_EXECUTABLE}"
        DIRECTORY
    )
    file(
        GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${EDIT_ATLAS_EXECUTABLE}"
        DIRECTORIES "${edit_atlas_executable_directory}"
        RESOLVED_DEPENDENCIES_VAR edit_atlas_resolved_dependencies
        UNRESOLVED_DEPENDENCIES_VAR edit_atlas_unresolved_dependencies
        PRE_EXCLUDE_REGEXES
            "api-ms-.*"
            "ext-ms-.*"
        POST_EXCLUDE_REGEXES
            ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
    )

    if(edit_atlas_unresolved_dependencies)
        list(
            JOIN edit_atlas_unresolved_dependencies
            "\n  "
            edit_atlas_unresolved_text
        )
        message(
            FATAL_ERROR
            "The staged application has unresolved runtime dependencies:\n  "
            "${edit_atlas_unresolved_text}"
        )
    endif()

    file(
        REAL_PATH "${EDIT_ATLAS_DEPLOYMENT_ROOT}"
        edit_atlas_deployment_root
    )
    foreach(
        edit_atlas_resolved_dependency
        IN LISTS edit_atlas_resolved_dependencies
    )
        file(
            REAL_PATH "${edit_atlas_resolved_dependency}"
            edit_atlas_resolved_dependency_path
        )
        cmake_path(
            IS_PREFIX edit_atlas_deployment_root
            "${edit_atlas_resolved_dependency_path}"
            NORMALIZE
            edit_atlas_dependency_is_deployed
        )
        if(NOT edit_atlas_dependency_is_deployed)
            message(
                FATAL_ERROR
                "Runtime dependency resolved outside the staged application: "
                "${edit_atlas_resolved_dependency_path}"
            )
        endif()
    endforeach()
endif()

edit_atlas_require_deployed_file(
    "the Edit Atlas third-party notices"
    "*THIRD_PARTY_NOTICES.md"
)
edit_atlas_require_deployed_file(
    "the Edit Atlas license"
    "*LICENSE"
)
edit_atlas_require_deployed_file(
    "the Qt Base license notices"
    "*qtbase-copyright"
)

message(STATUS "Verified staged Qt runtime and compliance materials.")
