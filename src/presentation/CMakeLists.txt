add_library(edit_atlas_presentation STATIC)
add_library(EditAtlas::Presentation ALIAS edit_atlas_presentation)

set_target_properties(
    edit_atlas_presentation
    PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_presentation
    PRIVATE
        application_state.cpp
        desktop_integration.cpp
        diagnostic_support.cpp
        diagnostic_text.cpp
        support_bundle_workflow.cpp
        timeline_document_workflow.cpp
        timeline_event_model.cpp
        translation.cpp
        include/edit_atlas/presentation/application_state.hpp
        include/edit_atlas/presentation/desktop_integration.hpp
        include/edit_atlas/presentation/diagnostic_support.hpp
        include/edit_atlas/presentation/diagnostic_text.hpp
        include/edit_atlas/presentation/support_bundle_workflow.hpp
        include/edit_atlas/presentation/timeline_document_workflow.hpp
        include/edit_atlas/presentation/timeline_event_model.hpp
        include/edit_atlas/presentation/translation.hpp
)

target_include_directories(
    edit_atlas_presentation
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(
    edit_atlas_presentation
    PUBLIC
        EditAtlas::Services
        EditAtlas::Support
        Qt6::Core
    PRIVATE
        Qt6::Concurrent
        Qt6::Gui
        spdlog::spdlog
)

target_compile_features(edit_atlas_presentation PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_presentation)
