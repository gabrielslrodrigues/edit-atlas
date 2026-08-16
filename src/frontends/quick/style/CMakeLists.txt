add_library(edit_atlas_quick_style STATIC)
add_library(EditAtlas::QuickStyle ALIAS edit_atlas_quick_style)

set_target_properties(
    edit_atlas_quick_style
    PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
)

set_source_files_properties(
    DesignTokens.qml
    Theme.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(
    edit_atlas_quick_style
    URI EditAtlasStyle
    VERSION 1.0
    QML_FILES
        Button.qml
        ComboBox.qml
        DesignTokens.qml
        Dialog.qml
        DialogButtonBox.qml
        Icon.qml
        Menu.qml
        MenuBar.qml
        MenuBarItem.qml
        MenuItem.qml
        MenuSeparator.qml
        Surface.qml
        TextField.qml
        Theme.qml
)

target_link_libraries(
    edit_atlas_quick_style
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
)

target_compile_features(edit_atlas_quick_style PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_quick_style)
