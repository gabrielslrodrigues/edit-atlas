add_executable(edit_atlas_app_tests)

target_sources(
    edit_atlas_app_tests
    PRIVATE
        application_test.cpp
        test_main.cpp
)

target_link_libraries(
    edit_atlas_app_tests
    PRIVATE
        EditAtlas::App
        GTest::gtest
)

target_compile_features(edit_atlas_app_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_app_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_app_tests
    DISCOVERY_TIMEOUT 30
)
