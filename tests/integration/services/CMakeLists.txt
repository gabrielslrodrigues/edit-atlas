add_executable(edit_atlas_services_integration_tests)

target_sources(
    edit_atlas_services_integration_tests
    PRIVATE
        built_in_formats_test.cpp
        timeline_document_export_service_test.cpp
        timeline_document_import_service_test.cpp
        test_main.cpp
        timeline_template_service_test.cpp
        timeline_video_inspection_service_test.cpp
)

target_link_libraries(
    edit_atlas_services_integration_tests
    PRIVATE
        EditAtlas::Services
        EditAtlas::MediaFixture
        GTest::gtest
)

target_compile_features(edit_atlas_services_integration_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_services_integration_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_services_integration_tests
    PROPERTIES LABELS integration
)
