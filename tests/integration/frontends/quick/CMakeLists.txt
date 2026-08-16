add_executable(edit_atlas_quick_frontend_integration_tests)

target_sources(
    edit_atlas_quick_frontend_integration_tests
    PRIVATE
        application_information_view_model_test.cpp
        application_shell_view_model_test.cpp
        spreadsheet_export_view_model_test.cpp
        support_bundle_view_model_test.cpp
        test_main.cpp
        timeline_configuration_view_model_test.cpp
)

target_compile_definitions(
    edit_atlas_quick_frontend_integration_tests
    PRIVATE
        EDIT_ATLAS_QUICK_FIXTURE_DIRECTORY="${edit_atlas_test_fixture_directory}/cmx3600"
)

target_link_libraries(
    edit_atlas_quick_frontend_integration_tests
    PRIVATE
        EditAtlas::Cmx3600
        EditAtlas::QuickFrontend
        GTest::gtest
        Qt6::Test
)

target_compile_features(
    edit_atlas_quick_frontend_integration_tests
    PRIVATE cxx_std_23
)
edit_atlas_set_project_warnings(edit_atlas_quick_frontend_integration_tests)
edit_atlas_normalize_macos_qt_dependencies(
    edit_atlas_quick_frontend_integration_tests
)
edit_atlas_deploy_windows_qt_platform_plugin(
    edit_atlas_quick_frontend_integration_tests
)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_quick_frontend_integration_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration
)

add_executable(
    edit_atlas_quick_qml_tests
    qml_test_main.cpp
)

set_target_properties(
    edit_atlas_quick_qml_tests
    PROPERTIES AUTOMOC ON
)

set(edit_atlas_quick_test_resource_directory
    "${PROJECT_SOURCE_DIR}/src/frontends/resources"
)
qt_add_resources(
    edit_atlas_quick_qml_tests
    "edit_atlas_quick_qml_test_icons"
    PREFIX
        "/"
    BASE
        "${edit_atlas_quick_test_resource_directory}"
    FILES
        "${edit_atlas_quick_test_resource_directory}/icons/edit_atlas.png"
)

target_compile_definitions(
    edit_atlas_quick_qml_tests
    PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/views"
)

target_link_libraries(
    edit_atlas_quick_qml_tests
    PRIVATE
        EditAtlas::QuickFrontend
        edit_atlas_quick_frontendplugin
        edit_atlas_quick_styleplugin
        Qt6::Qml
        Qt6::QuickControls2
        Qt6::QuickTest
        Qt6::Test
)

target_compile_features(edit_atlas_quick_qml_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_quick_qml_tests)
edit_atlas_deploy_windows_qt_platform_plugin(
    edit_atlas_quick_qml_tests
    Qt6::QMinimalIntegrationPlugin
)

add_test(
    NAME edit_atlas_quick_qml_tests
    COMMAND edit_atlas_quick_qml_tests
)
set_tests_properties(
    edit_atlas_quick_qml_tests
    PROPERTIES
        LABELS integration
        TIMEOUT 30
)
