add_executable(edit_atlas_media_integration_tests)

target_sources(
    edit_atlas_media_integration_tests
    PRIVATE
        media_fixture.cpp
        media_fixture.hpp
        video_decoder_test.cpp
)

target_include_directories(
    edit_atlas_media_integration_tests
    SYSTEM PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(
    edit_atlas_media_integration_tests
    PRIVATE
        EditAtlas::Media
        GTest::gtest_main
        ${FFMPEG_LIBRARIES}
)

target_compile_features(edit_atlas_media_integration_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_media_integration_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_media_integration_tests
    PROPERTIES LABELS integration
)
