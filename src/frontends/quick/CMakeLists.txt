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
    views/AboutDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS AboutDialog.qml
)

set_source_files_properties(
    views/Main.qml
    PROPERTIES
        QT_RESOURCE_ALIAS Main.qml
)

set_source_files_properties(
    views/SpreadsheetExportDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS SpreadsheetExportDialog.qml
)

set_source_files_properties(
    views/SupportBundleDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS SupportBundleDialog.qml
)

set_source_files_properties(
    views/DiagnosticsPanel.qml
    PROPERTIES
        QT_RESOURCE_ALIAS DiagnosticsPanel.qml
)

set_source_files_properties(
    views/EventProjectionDialog.qml
    PROPERTIES
        QT_RESOURCE_ALIAS EventProjectionDialog.qml
)

set_source_files_properties(
    views/TimelineConfigurationPanel.qml
    PROPERTIES
        QT_RESOURCE_ALIAS TimelineConfigurationPanel.qml
)

set_source_files_properties(
    views/TimelineFilterCondition.qml
    PROPERTIES
        QT_RESOURCE_ALIAS TimelineFilterCondition.qml
)

set_source_files_properties(
    views/TimelineTable.qml
    PROPERTIES
        QT_RESOURCE_ALIAS TimelineTable.qml
)

qt_add_qml_module(
    edit_atlas_quick_frontend
    URI EditAtlas.Frontends.Quick
    VERSION 1.0
    SOURCES
        application_information_view_model.cpp
        application_shell_view_model.cpp
        spreadsheet_export_view_model.cpp
        support_bundle_view_model.cpp
        timeline_configuration_view_model.cpp
        include/edit_atlas/frontends/quick/application_information_view_model.hpp
        include/edit_atlas/frontends/quick/application_shell_view_model.hpp
        include/edit_atlas/frontends/quick/spreadsheet_export_view_model.hpp
        include/edit_atlas/frontends/quick/support_bundle_view_model.hpp
        include/edit_atlas/frontends/quick/timeline_configuration_view_model.hpp
    QML_FILES
        views/AboutDialog.qml
        views/DiagnosticsPanel.qml
        views/EventProjectionDialog.qml
        views/Main.qml
        views/SpreadsheetExportDialog.qml
        views/SupportBundleDialog.qml
        views/TimelineConfigurationPanel.qml
        views/TimelineFilterCondition.qml
        views/TimelineTable.qml
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
        EditAtlas::Media
        EditAtlas::QuickStyle
        EditAtlas::Xlsx
        Qt6::Quick
        Qt6::QuickControls2
)

target_compile_features(edit_atlas_quick_frontend PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_quick_frontend)

function(edit_atlas_configure_quick_executable target)
    edit_atlas_add_application_icon_resource("${target}")

    target_link_libraries(
        "${target}"
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

    target_compile_features("${target}" PRIVATE cxx_std_23)
    edit_atlas_set_project_warnings("${target}")
    edit_atlas_deploy_windows_qt_platform_plugin(
        "${target}"
        Qt6::QWindowsIntegrationPlugin
    )
endfunction()

qt_add_executable(edit_atlas_quick main.cpp)
add_executable(EditAtlas::QuickApplication ALIAS edit_atlas_quick)
set_target_properties(edit_atlas_quick PROPERTIES OUTPUT_NAME "edit-atlas-quick")

if(APPLE AND EDIT_ATLAS_DEFAULT_FRONTEND STREQUAL "quick")
    # Qt records the QML deployment layout when it finalizes this directory.
    # Mark the selected target as a bundle before that deferred finalization.
    set_target_properties(edit_atlas_quick PROPERTIES MACOSX_BUNDLE ON)
endif()

edit_atlas_configure_quick_executable(edit_atlas_quick)

if(EDIT_ATLAS_BUILD_PACKAGING_APPLICATIONS)
    qt_add_executable(edit_atlas_quick_package main.cpp)
    add_executable(
        EditAtlas::QuickPackageApplication
        ALIAS edit_atlas_quick_package
    )
    set_target_properties(
        edit_atlas_quick_package
        PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY
                "${PROJECT_BINARY_DIR}/packaging/quick"
            RUNTIME_OUTPUT_DIRECTORY
                "${PROJECT_BINARY_DIR}/packaging/quick"
    )
    if(APPLE)
        # QML deployment records the bundle layout during deferred target
        # finalization, so the package target must be a bundle immediately.
        set_target_properties(
            edit_atlas_quick_package
            PROPERTIES MACOSX_BUNDLE ON
        )
    endif()
    edit_atlas_configure_quick_executable(edit_atlas_quick_package)
endif()
