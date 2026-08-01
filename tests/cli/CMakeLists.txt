find_package(minizip CONFIG REQUIRED)

add_executable(edit_atlas_cli_tests)

target_sources(
    edit_atlas_cli_tests
    PRIVATE
        application_test.cpp
        test_main.cpp
)

target_compile_definitions(
    edit_atlas_cli_tests
    PRIVATE
        EDIT_ATLAS_CLI_FIXTURE_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../formats/cmx3600/fixtures"
)

target_link_libraries(
    edit_atlas_cli_tests
    PRIVATE
        EditAtlas::CliFrontend
        GTest::gtest
        $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>
)

target_compile_features(edit_atlas_cli_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_cli_tests)

include(GoogleTest)
gtest_discover_tests(edit_atlas_cli_tests)

add_test(
    NAME edit_atlas_cli_end_to_end
    COMMAND
        "${CMAKE_COMMAND}"
        "-DEDIT_ATLAS_CLI_EXECUTABLE=$<TARGET_FILE:edit_atlas_cli>"
        "-DEDIT_ATLAS_CLI_FIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/../formats/cmx3600/fixtures/mixed_tracks.edl"
        "-DEDIT_ATLAS_CLI_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/edit-atlas-cli-end-to-end.xlsx"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/verify_cli_process.cmake"
)
