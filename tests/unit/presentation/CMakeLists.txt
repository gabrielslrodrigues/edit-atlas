add_executable(edit_atlas_presentation_tests)

target_sources(
    edit_atlas_presentation_tests
    PRIVATE
        diagnostic_model_test.cpp
        test_main.cpp
        timeline_event_model_test.cpp
        timeline_filter_model_test.cpp
)

target_link_libraries(
    edit_atlas_presentation_tests
    PRIVATE
        EditAtlas::Presentation
        GTest::gtest
)

target_compile_features(edit_atlas_presentation_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_presentation_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_presentation_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES LABELS unit
)
