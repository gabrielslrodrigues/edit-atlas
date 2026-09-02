if(NOT DEFINED EDIT_ATLAS_DEPLOYMENT_ROOT)
    message(FATAL_ERROR "EDIT_ATLAS_DEPLOYMENT_ROOT is required.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/VerifyDynamicQtBinary.cmake")

get_filename_component(
    edit_atlas_executable_directory
    "${EDIT_ATLAS_EXECUTABLE}"
    DIRECTORY
)
set(edit_atlas_cli_filename "edit-atlas-cli")
if(WIN32)
    string(APPEND edit_atlas_cli_filename ".exe")
endif()
set(
    edit_atlas_cli_executable
    "${edit_atlas_executable_directory}/${edit_atlas_cli_filename}"
)

if(NOT EXISTS "${edit_atlas_cli_executable}")
    message(
        FATAL_ERROR
        "The staged command-line executable is missing: "
        "${edit_atlas_cli_executable}"
    )
endif()

execute_process(
    COMMAND ${edit_atlas_dependency_command} "${edit_atlas_cli_executable}"
    RESULT_VARIABLE edit_atlas_cli_dependency_result
    OUTPUT_VARIABLE edit_atlas_cli_dependencies
    ERROR_VARIABLE edit_atlas_cli_dependency_error
)
if(NOT edit_atlas_cli_dependency_result EQUAL 0)
    message(
        FATAL_ERROR
        "Failed to inspect ${edit_atlas_cli_executable}:\n"
        "${edit_atlas_cli_dependency_error}"
    )
endif()
if(edit_atlas_cli_dependencies MATCHES "Qt6")
    message(
        FATAL_ERROR
        "The command-line executable unexpectedly depends on Qt:\n"
        "${edit_atlas_cli_dependencies}"
    )
endif()

execute_process(
    COMMAND "${edit_atlas_cli_executable}" --version
    RESULT_VARIABLE edit_atlas_cli_smoke_result
    OUTPUT_VARIABLE edit_atlas_cli_version
    ERROR_VARIABLE edit_atlas_cli_smoke_error
    TIMEOUT 15
)
if(NOT edit_atlas_cli_smoke_result EQUAL 0)
    message(
        FATAL_ERROR
        "The staged command-line executable failed its smoke test:\n"
        "${edit_atlas_cli_smoke_error}"
    )
endif()
if(NOT edit_atlas_cli_version MATCHES "^Edit Atlas [0-9]+\\.[0-9]+\\.[0-9]+")
    message(
        FATAL_ERROR
        "The staged command-line executable returned an unexpected version: "
        "${edit_atlas_cli_version}"
    )
endif()

message(
    STATUS
    "Verified Qt-free command-line executable: ${edit_atlas_cli_executable}"
)

if(LINUX)
    file(
        GLOB edit_atlas_private_runtime_candidates
        LIST_DIRECTORIES TRUE
        "${EDIT_ATLAS_DEPLOYMENT_ROOT}/lib/edit-atlas"
        "${EDIT_ATLAS_DEPLOYMENT_ROOT}/lib64/edit-atlas"
        "${EDIT_ATLAS_DEPLOYMENT_ROOT}/lib/*/edit-atlas"
    )
    set(edit_atlas_private_runtime_directories)
    foreach(
        edit_atlas_private_runtime_candidate
        IN LISTS edit_atlas_private_runtime_candidates
    )
        if(IS_DIRECTORY "${edit_atlas_private_runtime_candidate}")
            list(
                APPEND edit_atlas_private_runtime_directories
                "${edit_atlas_private_runtime_candidate}"
            )
        endif()
    endforeach()
    list(LENGTH edit_atlas_private_runtime_directories
        edit_atlas_private_runtime_directory_count)
    if(NOT edit_atlas_private_runtime_directory_count EQUAL 1)
        message(
            FATAL_ERROR
            "Expected exactly one private Linux runtime-library directory, "
            "found ${edit_atlas_private_runtime_directory_count}: "
            "${edit_atlas_private_runtime_directories}"
        )
    endif()

    list(GET edit_atlas_private_runtime_directories 0
        edit_atlas_private_runtime_directory)
    function(
        edit_atlas_require_private_linux_runtime_path
        description
        binary
        dependencies
    )
        get_filename_component(
            edit_atlas_runtime_binary_directory
            "${binary}"
            DIRECTORY
        )
        file(
            RELATIVE_PATH edit_atlas_private_runtime_relative_path
            "${edit_atlas_runtime_binary_directory}"
            "${edit_atlas_private_runtime_directory}"
        )
        set(
            edit_atlas_expected_runtime_path
            "$ORIGIN/${edit_atlas_private_runtime_relative_path}"
        )
        string(
            FIND "${dependencies}"
            "${edit_atlas_expected_runtime_path}"
            edit_atlas_runtime_path_position
        )
        if(edit_atlas_runtime_path_position EQUAL -1)
            message(
                FATAL_ERROR
                "${description} does not use the private Linux "
                "runtime-library directory "
                "${edit_atlas_expected_runtime_path}:\n${dependencies}"
            )
        endif()
    endfunction()

    edit_atlas_require_private_linux_runtime_path(
        "The graphical executable"
        "${EDIT_ATLAS_EXECUTABLE}"
        "${edit_atlas_dependencies}"
    )
    edit_atlas_require_private_linux_runtime_path(
        "The command-line executable"
        "${edit_atlas_cli_executable}"
        "${edit_atlas_cli_dependencies}"
    )

    # Every library the application needs must be bundled or genuinely
    # supplied by the target system. A compiler runtime such as Clang's
    # OpenMP library sits in a system directory on the build host and is
    # absent everywhere else, so it resolves during the build and then fails
    # to load on a user's machine. Checked for both frontends, because the
    # equivalent QML-plugin check below runs only for Qt Quick.
    file(
        GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${EDIT_ATLAS_EXECUTABLE}"
        DIRECTORIES "${edit_atlas_private_runtime_directory}"
        RESOLVED_DEPENDENCIES_VAR edit_atlas_application_resolved
        UNRESOLVED_DEPENDENCIES_VAR edit_atlas_application_unresolved
    )
    if(edit_atlas_application_unresolved)
        message(
            FATAL_ERROR
            "The staged application has unresolved runtime dependencies: "
            "${edit_atlas_application_unresolved}"
        )
    endif()

    if(EDIT_ATLAS_VERIFY_PRIVATE_RUNTIME_BOUNDARY)
        get_filename_component(
            edit_atlas_public_runtime_directory
            "${edit_atlas_private_runtime_directory}"
            DIRECTORY
        )
        file(
            GLOB edit_atlas_public_runtime_libraries
            LIST_DIRECTORIES FALSE
            "${edit_atlas_public_runtime_directory}/*.so"
            "${edit_atlas_public_runtime_directory}/*.so.*"
        )
        if(edit_atlas_public_runtime_libraries)
            message(
                FATAL_ERROR
                "The package installs runtime libraries directly in the "
                "public library directory: "
                "${edit_atlas_public_runtime_libraries}"
            )
        endif()
    endif()
endif()

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

function(edit_atlas_require_qml_import qml_import)
    set(
        edit_atlas_qmldir
        "${edit_atlas_qml_root}/${qml_import}/qmldir"
    )
    if(NOT EXISTS "${edit_atlas_qmldir}")
        message(
            FATAL_ERROR
            "The staged application is missing the ${qml_import} QML import "
            "under ${edit_atlas_qml_root}."
        )
    endif()
endfunction()

function(edit_atlas_require_qt_library library)
    if(WIN32)
        edit_atlas_require_deployed_file(
            "Qt ${library}"
            "*Qt6${library}.dll"
            "*Qt6${library}d.dll"
        )
    elseif(APPLE)
        edit_atlas_require_deployed_file(
            "Qt ${library}"
            "*libQt6${library}.dylib"
            "*libQt6${library}.*.dylib"
        )
    elseif(LINUX)
        edit_atlas_require_deployed_file(
            "Qt ${library}"
            "*libQt6${library}.so"
            "*libQt6${library}.so.*"
        )
    endif()
endfunction()

foreach(edit_atlas_common_qt_library IN ITEMS Core Gui)
    edit_atlas_require_qt_library("${edit_atlas_common_qt_library}")
endforeach()

if(edit_atlas_detected_frontend STREQUAL "quick")
    if(WIN32)
        set(edit_atlas_qml_root "${EDIT_ATLAS_DEPLOYMENT_ROOT}/Qt6/qml")
    elseif(APPLE)
        get_filename_component(
            edit_atlas_macos_contents_directory
            "${edit_atlas_executable_directory}"
            DIRECTORY
        )
        set(
            edit_atlas_qml_root
            "${edit_atlas_macos_contents_directory}/Resources/qml"
        )
    elseif(LINUX)
        set(
            edit_atlas_qml_root
            "${edit_atlas_private_runtime_directory}/Qt6/qml"
        )
    endif()

    foreach(
        edit_atlas_qt_quick_library
        IN ITEMS Network Qml Quick QuickControls2 QuickTemplates2
    )
        edit_atlas_require_qt_library("${edit_atlas_qt_quick_library}")
    endforeach()

    foreach(
        edit_atlas_qml_import
        IN ITEMS
            QtQml
            QtQuick
            QtQuick/Controls
            QtQuick/Dialogs
            QtQuick/Layouts
    )
        edit_atlas_require_qml_import("${edit_atlas_qml_import}")
    endforeach()

    foreach(
        edit_atlas_qml_plugin
        IN ITEMS
            qmlplugin
            qtquick2plugin
            qtquickcontrols2plugin
            qtquickdialogsplugin
            qquicklayoutsplugin
            qtquicktemplates2plugin
    )
        edit_atlas_require_deployed_file(
            "the ${edit_atlas_qml_plugin} QML plugin"
            "*${edit_atlas_qml_plugin}.dll"
            "*${edit_atlas_qml_plugin}d.dll"
            "*lib${edit_atlas_qml_plugin}.dylib"
            "*lib${edit_atlas_qml_plugin}.so"
        )
    endforeach()
elseif(edit_atlas_detected_frontend STREQUAL "widgets")
    edit_atlas_require_qt_library(Widgets)
else()
    message(
        FATAL_ERROR
        "Unsupported deployed frontend: ${edit_atlas_detected_frontend}"
    )
endif()

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
        abseil
        spdlog
        fmt
        FFmpeg-avcodec
        FFmpeg-avformat
        FFmpeg-avutil
        FFmpeg-swscale
        libpng
        libxlsxwriter
        minizip
        re2
    )
    set(
        edit_atlas_third_party_runtime_patterns
        "*abseil*.dll"
        "*spdlog*.dll"
        "*fmt*.dll"
        "*avcodec-*.dll"
        "*avformat-*.dll"
        "*avutil-*.dll"
        "*swscale-*.dll"
        "*png*.dll"
        "*xlsxwriter*.dll"
        "*minizip*.dll"
        "*re2*.dll"
    )
elseif(APPLE)
    edit_atlas_require_deployed_file(
        "the Cocoa Qt platform plugin"
        "*libqcocoa.dylib"
    )

    file(
        GLOB edit_atlas_macos_framework_entries
        LIST_DIRECTORIES TRUE
        "${EDIT_ATLAS_DEPLOYMENT_ROOT}/edit-atlas.app/Contents/Frameworks/*"
    )
    foreach(
        edit_atlas_macos_framework_entry
        IN LISTS edit_atlas_macos_framework_entries
    )
        if(IS_DIRECTORY "${edit_atlas_macos_framework_entry}")
            message(
                FATAL_ERROR
                "The staged macOS Frameworks directory contains an "
                "unexpected subdirectory: "
                "${edit_atlas_macos_framework_entry}"
            )
        endif()
    endforeach()

    set(
        edit_atlas_third_party_runtime_descriptions
        spdlog
        fmt
        FFmpeg-avcodec
        FFmpeg-avformat
        FFmpeg-avutil
        FFmpeg-swscale
        libpng
        libxlsxwriter
        minizip
        re2
    )
    set(
        edit_atlas_third_party_runtime_patterns
        "*libspdlog*.dylib"
        "*libfmt*.dylib"
        "*libavcodec*.dylib"
        "*libavformat*.dylib"
        "*libavutil*.dylib"
        "*libswscale*.dylib"
        "*libpng*.dylib"
        "*libxlsxwriter*.dylib"
        "*libminizip*.dylib"
        "*libre2*.dylib"
    )
elseif(LINUX)
    edit_atlas_require_deployed_file(
        "the XCB Qt platform plugin"
        "*libqxcb.so"
    )
    edit_atlas_require_deployed_file(
        "a Wayland Qt platform plugin"
        "*libqwayland*.so"
    )

    file(
        GLOB_RECURSE edit_atlas_linux_qt_plugins
        LIST_DIRECTORIES FALSE
        "${edit_atlas_private_runtime_directory}/Qt6/plugins/*.so"
    )
    foreach(
        edit_atlas_linux_qt_plugin
        IN LISTS edit_atlas_linux_qt_plugins
    )
        execute_process(
            COMMAND
                ${edit_atlas_dependency_command}
                "${edit_atlas_linux_qt_plugin}"
            RESULT_VARIABLE edit_atlas_plugin_dependency_result
            OUTPUT_VARIABLE edit_atlas_plugin_dependencies
            ERROR_VARIABLE edit_atlas_plugin_dependency_error
        )
        if(NOT edit_atlas_plugin_dependency_result EQUAL 0)
            message(
                FATAL_ERROR
                "Failed to inspect ${edit_atlas_linux_qt_plugin}:\n"
                "${edit_atlas_plugin_dependency_error}"
            )
        endif()
        edit_atlas_require_private_linux_runtime_path(
            "The Qt plugin ${edit_atlas_linux_qt_plugin}"
            "${edit_atlas_linux_qt_plugin}"
            "${edit_atlas_plugin_dependencies}"
        )
    endforeach()

    if(edit_atlas_detected_frontend STREQUAL "quick")
        file(
            GLOB_RECURSE edit_atlas_linux_qml_plugins
            LIST_DIRECTORIES FALSE
            "${EDIT_ATLAS_DEPLOYMENT_ROOT}/*.so"
        )
        list(
            FILTER edit_atlas_linux_qml_plugins
            INCLUDE REGEX "/qml/"
        )
        if(NOT edit_atlas_linux_qml_plugins)
            message(
                FATAL_ERROR
                "The staged Qt Quick application has no deployed QML "
                "plugins."
            )
        endif()
        file(
            GET_RUNTIME_DEPENDENCIES
            MODULES ${edit_atlas_linux_qml_plugins}
            DIRECTORIES "${edit_atlas_private_runtime_directory}"
            RESOLVED_DEPENDENCIES_VAR
                edit_atlas_linux_qml_runtime_dependencies
            UNRESOLVED_DEPENDENCIES_VAR
                edit_atlas_linux_qml_unresolved_dependencies
        )
        if(edit_atlas_linux_qml_unresolved_dependencies)
            message(
                FATAL_ERROR
                "The staged QML plugins have unresolved runtime "
                "dependencies: "
                "${edit_atlas_linux_qml_unresolved_dependencies}"
            )
        endif()
        foreach(
            edit_atlas_linux_qml_plugin
            IN LISTS edit_atlas_linux_qml_plugins
        )
            execute_process(
                COMMAND
                    ${edit_atlas_dependency_command}
                    "${edit_atlas_linux_qml_plugin}"
                RESULT_VARIABLE edit_atlas_qml_dependency_result
                OUTPUT_VARIABLE edit_atlas_qml_dependencies
                ERROR_VARIABLE edit_atlas_qml_dependency_error
            )
            if(NOT edit_atlas_qml_dependency_result EQUAL 0)
                message(
                    FATAL_ERROR
                    "Failed to inspect ${edit_atlas_linux_qml_plugin}:\n"
                    "${edit_atlas_qml_dependency_error}"
                )
            endif()
            edit_atlas_require_private_linux_runtime_path(
                "The QML plugin ${edit_atlas_linux_qml_plugin}"
                "${edit_atlas_linux_qml_plugin}"
                "${edit_atlas_qml_dependencies}"
            )
        endforeach()
    endif()

    set(
        edit_atlas_third_party_runtime_descriptions
        spdlog
        fmt
        FFmpeg-avcodec
        FFmpeg-avformat
        FFmpeg-avutil
        FFmpeg-swscale
        libpng
        libxlsxwriter
        minizip
        re2
    )
    set(
        edit_atlas_third_party_runtime_patterns
        "*libspdlog.so*"
        "*libfmt.so*"
        "*libavcodec.so*"
        "*libavformat.so*"
        "*libavutil.so*"
        "*libswscale.so*"
        "*libpng*.so*"
        "*libxlsxwriter.so*"
        "*libminizip.so*"
        "*libre2.so*"
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
        GLOB_RECURSE edit_atlas_windows_qml_modules
        LIST_DIRECTORIES FALSE
        "${EDIT_ATLAS_DEPLOYMENT_ROOT}/*.dll"
    )
    list(
        FILTER edit_atlas_windows_qml_modules
        INCLUDE REGEX "[/\\\\]qml[/\\\\]"
    )
    set(edit_atlas_windows_module_arguments)
    if(edit_atlas_windows_qml_modules)
        list(
            APPEND edit_atlas_windows_module_arguments
            MODULES ${edit_atlas_windows_qml_modules}
        )
    endif()

    file(
        GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${EDIT_ATLAS_EXECUTABLE}"
        ${edit_atlas_windows_module_arguments}
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
foreach(
    edit_atlas_qt_notice_package
    IN ITEMS
        qtbase
        qtdeclarative
        qtlanguageserver
        qtshadertools
        qtsvg
)
    edit_atlas_require_deployed_file(
        "the ${edit_atlas_qt_notice_package} license notices"
        "*${edit_atlas_qt_notice_package}-copyright"
    )
endforeach()
foreach(
    edit_atlas_notice_package
    IN ITEMS
        abseil
        cli11
        ffmpeg
        fmt
        libpng
        libxlsxwriter
        minizip
        nlohmann-json
        re2
        spdlog
        zlib
)
    edit_atlas_require_deployed_file(
        "the ${edit_atlas_notice_package} license notices"
        "*${edit_atlas_notice_package}-copyright"
    )
endforeach()
edit_atlas_require_deployed_file(
    "the Inter Open Font License text"
    "*Inter-LICENSE.txt"
)
edit_atlas_require_deployed_file(
    "the FFmpeg corresponding source offer"
    "*FFMPEG_SOURCE_OFFER.md"
)
edit_atlas_require_deployed_file(
    "the Qt corresponding source offer"
    "*QT_SOURCE_OFFER.md"
)

message(STATUS "Verified staged application runtime and compliance materials.")
