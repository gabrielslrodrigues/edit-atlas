qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(QT_QML_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/qml")

add_subdirectory(style)

add_library(edit_atlas_quick_frontend STATIC)
add_library(EditAtlas::QuickFrontend ALIAS edit_atlas_quick_frontend)

set_target_properties(
    edit_atlas_quick_frontend
    PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
)

set_source_files_properties(
    qml/Main.qml
    PROPERTIES
        QT_RESOURCE_ALIAS Main.qml
)

qt_add_qml_module(
    edit_atlas_quick_frontend
    URI EditAtlas.Frontends.Quick
    VERSION 1.0
    SOURCES
        application_shell_view_model.cpp
        include/edit_atlas/frontends/quick/application_shell_view_model.hpp
    QML_FILES
        qml/Main.qml
)

target_include_directories(
    edit_atlas_quick_frontend
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/edit_atlas/frontends/quick>"
)

target_link_libraries(
    edit_atlas_quick_frontend
    PUBLIC
        EditAtlas::Presentation
        Qt6::Qml
    PRIVATE
        EditAtlas::Cmx3600
        EditAtlas::QuickStyle
        Qt6::Quick
        Qt6::QuickControls2
)

target_compile_features(edit_atlas_quick_frontend PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_quick_frontend)

qt_add_executable(edit_atlas_quick main.cpp)
add_executable(EditAtlas::QuickApplication ALIAS edit_atlas_quick)
set_target_properties(edit_atlas_quick PROPERTIES OUTPUT_NAME "edit-atlas-quick")

set(edit_atlas_quick_resource_directory
    "${PROJECT_SOURCE_DIR}/src/frontends/resources"
)
qt_add_resources(
    edit_atlas_quick
    "edit_atlas_quick_icons"
    PREFIX
        "/"
    BASE
        "${edit_atlas_quick_resource_directory}"
    FILES
        "${edit_atlas_quick_resource_directory}/icons/edit_atlas.png"
)

target_link_libraries(
    edit_atlas_quick
    PRIVATE
        EditAtlas::Core
        EditAtlas::Media
        EditAtlas::QuickFrontend
        EditAtlas::Services
        EditAtlas::Support
        edit_atlas_quick_frontendplugin
        edit_atlas_quick_styleplugin
        Qt6::Gui
        Qt6::Qml
        Qt6::QuickControls2
        spdlog::spdlog
)

target_compile_features(edit_atlas_quick PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_quick)
edit_atlas_deploy_windows_qt_platform_plugin(edit_atlas_quick)
