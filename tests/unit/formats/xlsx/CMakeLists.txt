find_package(minizip CONFIG REQUIRED)

add_executable(edit_atlas_xlsx_tests)

target_sources(
    edit_atlas_xlsx_tests
    PRIVATE xlsx_exporter_test.cpp
)

target_link_libraries(
    edit_atlas_xlsx_tests
    PRIVATE
        EditAtlas::Xlsx
        GTest::gtest_main
        $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>
)

target_compile_features(edit_atlas_xlsx_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_xlsx_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_xlsx_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS unit
)
