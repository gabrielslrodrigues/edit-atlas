add_executable(edit_atlas_services_tests)

target_sources(
    edit_atlas_services_tests
    PRIVATE
        built_in_formats_test.cpp
        document_export_service_test.cpp
        document_import_service_test.cpp
        test_main.cpp
        timeline_filter_test.cpp
)

target_link_libraries(
    edit_atlas_services_tests
    PRIVATE
        EditAtlas::Services
        GTest::gtest
)

target_compile_features(edit_atlas_services_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_services_tests)

include(GoogleTest)
gtest_discover_tests(edit_atlas_services_tests)
