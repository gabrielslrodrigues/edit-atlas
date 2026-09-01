find_package(minizip CONFIG REQUIRED)

add_executable(edit_atlas_support_tests)

target_sources(
    edit_atlas_support_tests
    PRIVATE
        application_logging_test.cpp
        support_bundle_test.cpp
        test_main.cpp
)

target_link_libraries(
    edit_atlas_support_tests
    PRIVATE
        EditAtlas::Support
        GTest::gtest
        $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>
        spdlog::spdlog
)

target_compile_features(edit_atlas_support_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_support_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_support_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS unit
)
