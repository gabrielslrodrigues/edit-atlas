find_package(minizip CONFIG REQUIRED)

add_executable(edit_atlas_cli_frontend_integration_tests)

target_sources(
    edit_atlas_cli_frontend_integration_tests
    PRIVATE
        application_test.cpp
        test_main.cpp
)

target_compile_definitions(
    edit_atlas_cli_frontend_integration_tests
    PRIVATE
        EDIT_ATLAS_FRONTENDS_CLI_FIXTURE_DIRECTORY="${edit_atlas_test_fixture_directory}/cmx3600"
)

target_link_libraries(
    edit_atlas_cli_frontend_integration_tests
    PRIVATE
        EditAtlas::CliFrontend
        EditAtlas::MediaFixture
        GTest::gtest
        $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>
)

target_compile_features(edit_atlas_cli_frontend_integration_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_cli_frontend_integration_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_cli_frontend_integration_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS integration
)
