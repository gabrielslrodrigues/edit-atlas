#ifndef EDIT_ATLAS_TEST_MEDIA_FIXTURE_HPP_
#define EDIT_ATLAS_TEST_MEDIA_FIXTURE_HPP_

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace edit_atlas::media::test {

enum class FixtureVideoCodec {
    kMpeg2Video,
    kRawVideo,
};

struct VideoFixtureOptions final {
    FixtureVideoCodec codec = FixtureVideoCodec::kMpeg2Video;
    std::int32_t frame_rate_numerator = 25;
    std::int32_t frame_rate_denominator = 1;
    std::int32_t frame_count = 3;
    std::optional<std::string> starting_timecode;
};

[[nodiscard]] std::expected<void, std::string>
WriteVideoFixture(const std::filesystem::path &path,
                  std::string_view container_name,
                  const VideoFixtureOptions &options);

} // namespace edit_atlas::media::test

#endif // EDIT_ATLAS_TEST_MEDIA_FIXTURE_HPP_
