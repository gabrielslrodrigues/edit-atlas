#ifndef EDIT_ATLAS_TESTS_UNIT_MEDIA_MEDIA_FIXTURE_HPP_
#define EDIT_ATLAS_TESTS_UNIT_MEDIA_MEDIA_FIXTURE_HPP_

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace edit_atlas::media::test {

enum class FixtureVideoCodec {
    kMpeg2Video,
    kRawVideo,
};

[[nodiscard]] std::expected<void, std::string>
WriteVideoFixture(const std::filesystem::path &path,
                  std::string_view container_name, FixtureVideoCodec codec);

} // namespace edit_atlas::media::test

#endif // EDIT_ATLAS_TESTS_UNIT_MEDIA_MEDIA_FIXTURE_HPP_
