add_executable(edit_atlas_app_integration_tests)

target_sources(
    edit_atlas_app_integration_tests
    PRIVATE
        accessibility_contract_test.cpp
        application_state_test.cpp
        test_main.cpp
)

target_compile_definitions(
    edit_atlas_app_integration_tests
    PRIVATE
        EDIT_ATLAS_INTEGRATION_FIXTURE_DIRECTORY="${edit_atlas_test_fixture_directory}/cmx3600"
)

target_include_directories(
    edit_atlas_app_integration_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src/app"
)

target_link_libraries(
    edit_atlas_app_integration_tests
    PRIVATE
        EditAtlas::App
        GTest::gtest
        Qt6::Test
)

target_compile_features(edit_atlas_app_integration_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_app_integration_tests)
edit_atlas_normalize_macos_qt_dependencies(
    edit_atlas_app_integration_tests
)
edit_atlas_deploy_windows_qt_platform_plugin(
    edit_atlas_app_integration_tests
)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_app_integration_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration
)
