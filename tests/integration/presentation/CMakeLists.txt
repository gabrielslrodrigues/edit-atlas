find_package(minizip CONFIG REQUIRED)

add_executable(edit_atlas_presentation_integration_tests)

target_sources(
    edit_atlas_presentation_integration_tests
    PRIVATE
        test_main.cpp
        timeline_document_view_model_test.cpp
        timeline_document_workflow_test.cpp
        timeline_template_view_model_test.cpp
)

target_compile_definitions(
    edit_atlas_presentation_integration_tests
    PRIVATE
        EDIT_ATLAS_INTEGRATION_FIXTURE_DIRECTORY="${edit_atlas_test_fixture_directory}/cmx3600"
)

target_link_libraries(
    edit_atlas_presentation_integration_tests
    PRIVATE
        EditAtlas::Presentation
        GTest::gtest
        $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>
        Qt6::Test
)

target_compile_features(
    edit_atlas_presentation_integration_tests
    PRIVATE
        cxx_std_23
)
edit_atlas_set_project_warnings(
    edit_atlas_presentation_integration_tests
)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_presentation_integration_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration
)
