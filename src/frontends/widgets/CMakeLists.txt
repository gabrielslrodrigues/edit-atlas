add_library(edit_atlas_widgets_frontend STATIC)
add_library(EditAtlas::WidgetsFrontend ALIAS edit_atlas_widgets_frontend)

set_target_properties(
    edit_atlas_widgets_frontend
    PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_widgets_frontend
    PRIVATE
        accessibility.cpp
        accessibility.hpp
        application_style.cpp
        application_menu_bar.cpp
        timeline_document_controller.cpp
        timeline_document_view.cpp
        timeline_template_controller.cpp
        timeline_template_controller.hpp
        event_projection_dialog.cpp
        event_projection_dialog.hpp
        event_projection_widget.cpp
        event_projection_widget.hpp
        include/edit_atlas/frontends/widgets/application_style.hpp
        include/edit_atlas/frontends/widgets/application_menu_bar.hpp
        include/edit_atlas/frontends/widgets/timeline_document_controller.hpp
        include/edit_atlas/frontends/widgets/timeline_document_view.hpp
        include/edit_atlas/frontends/widgets/main_window.hpp
        include/edit_atlas/frontends/widgets/spreadsheet_export_options_dialog.hpp
        include/edit_atlas/frontends/widgets/support_bundle_controller.hpp
        main_window.cpp
        spreadsheet_export_options_dialog.cpp
        support_bundle_controller.cpp
        timeline_filter_widget.cpp
        timeline_filter_widget.hpp
)

target_include_directories(
    edit_atlas_widgets_frontend
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    edit_atlas_widgets_frontend
    PUBLIC
        EditAtlas::Services
        EditAtlas::Support
        EditAtlas::Presentation
        Qt6::Widgets
    PRIVATE
        spdlog::spdlog
)

target_compile_features(edit_atlas_widgets_frontend PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_widgets_frontend)

qt_add_resources(
    edit_atlas_widgets_frontend
    "edit_atlas_widgets_frontend_style"
    PREFIX
        "/"
    BASE
        "resources"
    FILES
        "resources/styles/edit_atlas.qss"
)

qt_add_executable(edit_atlas_widgets main.cpp)
add_executable(EditAtlas::WidgetsApplication ALIAS edit_atlas_widgets)
set_target_properties(edit_atlas_widgets PROPERTIES OUTPUT_NAME "edit-atlas-widgets")

target_link_libraries(
    edit_atlas_widgets
    PRIVATE
        EditAtlas::WidgetsFrontend
        EditAtlas::Media
        spdlog::spdlog
)

target_compile_features(edit_atlas_widgets PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_widgets)
edit_atlas_deploy_windows_qt_platform_plugin(
    edit_atlas_widgets
    Qt6::QWindowsIntegrationPlugin
)
