find_package(minizip CONFIG REQUIRED)

add_executable(edit_atlas_app_integration_tests)

target_sources(
    edit_atlas_app_integration_tests
    PRIVATE
        accessibility_contract_test.cpp
        application_state_test.cpp
        test_main.cpp
        timeline_document_workflow_test.cpp
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
        $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>
        Qt6::Test
)

target_compile_features(edit_atlas_app_integration_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_app_integration_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_app_integration_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration
)
