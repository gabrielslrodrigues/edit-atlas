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
    QML_FILES
        qml/Main.qml
)

target_link_libraries(
    edit_atlas_quick_frontend
    PRIVATE
        EditAtlas::Presentation
        EditAtlas::QuickStyle
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
)

target_compile_features(edit_atlas_quick_frontend PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_quick_frontend)

qt_add_executable(edit_atlas_quick main.cpp)
add_executable(EditAtlas::QuickApplication ALIAS edit_atlas_quick)
set_target_properties(edit_atlas_quick PROPERTIES OUTPUT_NAME "edit-atlas-quick")

target_link_libraries(
    edit_atlas_quick
    PRIVATE
        edit_atlas_quick_frontendplugin
        edit_atlas_quick_styleplugin
        Qt6::Gui
        Qt6::Qml
        Qt6::QuickControls2
)

target_compile_features(edit_atlas_quick PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_quick)
edit_atlas_deploy_windows_qt_platform_plugin(edit_atlas_quick)
