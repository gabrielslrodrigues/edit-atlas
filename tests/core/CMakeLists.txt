add_executable(edit_atlas_core_tests)

target_sources(edit_atlas_core_tests PRIVATE version_test.cpp)

target_link_libraries(
    edit_atlas_core_tests
    PRIVATE
        EditAtlas::Core
        GTest::gtest_main
)

target_compile_features(edit_atlas_core_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_core_tests)

include(GoogleTest)
gtest_discover_tests(edit_atlas_core_tests)
