add_executable(edit_atlas_services_unit_tests)

target_sources(
    edit_atlas_services_unit_tests
    PRIVATE
        timeline_filter_test.cpp
        timeline_frame_extraction_service_test.cpp
        timeline_video_inspection_service_test.cpp
)

target_link_libraries(
    edit_atlas_services_unit_tests
    PRIVATE
        EditAtlas::Services
        GTest::gtest_main
)

target_compile_features(edit_atlas_services_unit_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_services_unit_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_services_unit_tests
    PROPERTIES LABELS unit
)
