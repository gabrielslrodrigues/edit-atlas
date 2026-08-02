add_library(edit_atlas_app STATIC)
add_library(EditAtlas::App ALIAS edit_atlas_app)

set_target_properties(
    edit_atlas_app
    PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_app
    PRIVATE
        application_style.cpp
        application_menu_bar.cpp
        desktop_integration.cpp
        diagnostic_support.cpp
        diagnostic_text.cpp
        timeline_document_controller.cpp
        timeline_document_view.cpp
        timeline_document_workflow.cpp
        timeline_template_controller.cpp
        timeline_template_controller.hpp
        event_projection_dialog.cpp
        event_projection_dialog.hpp
        event_projection_widget.cpp
        event_projection_widget.hpp
        include/edit_atlas/app/application_style.hpp
        include/edit_atlas/app/application_menu_bar.hpp
        include/edit_atlas/app/desktop_integration.hpp
        include/edit_atlas/app/diagnostic_support.hpp
        include/edit_atlas/app/diagnostic_text.hpp
        include/edit_atlas/app/timeline_document_controller.hpp
        include/edit_atlas/app/timeline_document_view.hpp
        include/edit_atlas/app/timeline_document_workflow.hpp
        include/edit_atlas/app/main_window.hpp
        include/edit_atlas/app/spreadsheet_export_options_dialog.hpp
        include/edit_atlas/app/support_bundle_controller.hpp
        include/edit_atlas/app/support_bundle_workflow.hpp
        include/edit_atlas/app/timeline_event_model.hpp
        include/edit_atlas/app/translation.hpp
        main_window.cpp
        spreadsheet_export_options_dialog.cpp
        support_bundle_controller.cpp
        support_bundle_workflow.cpp
        timeline_filter_widget.cpp
        timeline_filter_widget.hpp
        timeline_event_model.cpp
        translation.cpp
)

target_include_directories(
    edit_atlas_app
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    edit_atlas_app
    PUBLIC
        EditAtlas::Services
        EditAtlas::Support
        Qt6::Widgets
    PRIVATE
        Qt6::Concurrent
        spdlog::spdlog
)

target_compile_features(edit_atlas_app PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_app)

qt_add_executable(edit_atlas main.cpp)

qt_add_resources(
    edit_atlas
    "edit_atlas_icons"
    PREFIX
        "/"
    BASE
        "resources"
    FILES
        "resources/icons/edit_atlas.png"
)

if(WIN32)
    set(edit_atlas_windows_icon
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/edit_atlas.ico"
    )
    configure_file(
        "resources/icons/edit_atlas.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/edit_atlas.rc"
        @ONLY
    )
    target_sources(
        edit_atlas
        PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/edit_atlas.rc"
    )
elseif(APPLE)
    set(edit_atlas_macos_icon
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/edit_atlas.icns"
    )
    set_source_files_properties(
        "${edit_atlas_macos_icon}"
        PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(edit_atlas PRIVATE "${edit_atlas_macos_icon}")
    set_target_properties(
        edit_atlas
        PROPERTIES
            MACOSX_BUNDLE_ICON_FILE "edit_atlas.icns"
    )
endif()

set_target_properties(
    edit_atlas
    PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_BUNDLE_NAME "Edit Atlas"
        MACOSX_BUNDLE_GUI_IDENTIFIER
            "com.github.gabrielslrodrigues.edit-atlas"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        OUTPUT_NAME "edit-atlas"
        WIN32_EXECUTABLE ON
)

target_link_libraries(
    edit_atlas
    PRIVATE
        EditAtlas::App
        spdlog::spdlog
)

target_compile_features(edit_atlas PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas)

if(WIN32)
    add_custom_command(
        TARGET edit_atlas
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:edit_atlas>/platforms"
        COMMAND
            "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
            "$<TARGET_FILE_DIR:edit_atlas>/platforms/$<TARGET_FILE_NAME:Qt6::QWindowsIntegrationPlugin>"
        VERBATIM
    )
endif()

qt_add_translations(
    TARGETS edit_atlas_app
    SOURCE_TARGETS edit_atlas_app
    TS_FILES translations/edit_atlas_pt_BR.ts
    RESOURCE_PREFIX "/i18n"
)

set(edit_atlas_runtime_dependency_arguments)
if(WIN32)
    list(
        APPEND edit_atlas_runtime_dependency_arguments
        RUNTIME_DEPENDENCY_SET edit_atlas_runtime_dependencies
    )
endif()

install(
    TARGETS edit_atlas
    ${edit_atlas_runtime_dependency_arguments}
    BUNDLE
        DESTINATION "."
        COMPONENT Runtime
    RUNTIME
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Runtime
)

if(WIN32)
    install(
        RUNTIME_DEPENDENCY_SET edit_atlas_runtime_dependencies
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Runtime
        DIRECTORIES
            "$<IF:$<CONFIG:Debug>,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin>"
        PRE_EXCLUDE_REGEXES
            "api-ms-.*"
            "ext-ms-.*"
        POST_EXCLUDE_REGEXES
            ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
    )
endif()

if(APPLE)
    install(
        FILES
            "$<TARGET_FILE:spdlog::spdlog>"
            "$<TARGET_FILE:fmt::fmt>"
        DESTINATION "edit-atlas.app/Contents/Frameworks"
        COMPONENT Runtime
    )
endif()

if(LINUX)
    install(
        FILES "resources/linux/edit-atlas.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
        COMPONENT Runtime
    )
    install(
        FILES "resources/icons/edit_atlas.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/1024x1024/apps"
        RENAME "edit-atlas.png"
        COMPONENT Runtime
    )
endif()

set(edit_atlas_deploy_arguments)
if(APPLE)
    list(
        APPEND edit_atlas_deploy_arguments
        DEPLOY_TOOL_OPTIONS
            "-libpath=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(
        APPEND edit_atlas_deploy_arguments
        INCLUDE_PLUGINS
            qwayland
            qxcb
    )
endif()

qt_generate_deploy_app_script(
    TARGET edit_atlas
    OUTPUT_SCRIPT edit_atlas_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    ${edit_atlas_deploy_arguments}
)
install(
    SCRIPT "${edit_atlas_deploy_script}"
    COMPONENT Runtime
)
