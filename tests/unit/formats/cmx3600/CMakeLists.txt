add_executable(edit_atlas_cmx3600_tests)

target_sources(
    edit_atlas_cmx3600_tests
    PRIVATE cmx3600_importer_test.cpp
)

target_link_libraries(
    edit_atlas_cmx3600_tests
    PRIVATE
        EditAtlas::Cmx3600
        GTest::gtest_main
)

target_compile_definitions(
    edit_atlas_cmx3600_tests
    PRIVATE
        EDIT_ATLAS_CMX3600_FIXTURE_DIR="${edit_atlas_test_fixture_directory}/cmx3600"
)

target_compile_features(edit_atlas_cmx3600_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_cmx3600_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_cmx3600_tests
    PROPERTIES LABELS unit
)
