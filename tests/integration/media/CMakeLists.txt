add_library(edit_atlas_media_fixture STATIC)
add_library(EditAtlas::MediaFixture ALIAS edit_atlas_media_fixture)

target_sources(
    edit_atlas_media_fixture
    PRIVATE media_fixture.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/edit_atlas/test/media_fixture.hpp
)

target_include_directories(
    edit_atlas_media_fixture
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(
    edit_atlas_media_fixture
    SYSTEM PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(edit_atlas_media_fixture PRIVATE ${FFMPEG_LIBRARIES})
target_compile_features(edit_atlas_media_fixture PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_media_fixture)

add_executable(edit_atlas_e2e_media_fixture_generator)

target_sources(
    edit_atlas_e2e_media_fixture_generator
    PRIVATE e2e_fixture_generator.cpp
)

target_link_libraries(
    edit_atlas_e2e_media_fixture_generator
    PRIVATE EditAtlas::MediaFixture
)

target_compile_features(
    edit_atlas_e2e_media_fixture_generator
    PRIVATE cxx_std_23
)
edit_atlas_set_project_warnings(edit_atlas_e2e_media_fixture_generator)

add_executable(edit_atlas_media_integration_tests)

target_sources(
    edit_atlas_media_integration_tests
    PRIVATE video_decoder_test.cpp
)

target_link_libraries(
    edit_atlas_media_integration_tests
    PRIVATE
        EditAtlas::Media
        EditAtlas::MediaFixture
        GTest::gtest_main
)

target_compile_features(edit_atlas_media_integration_tests PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_media_integration_tests)

include(GoogleTest)
gtest_discover_tests(
    edit_atlas_media_integration_tests
    PROPERTIES LABELS integration
)
