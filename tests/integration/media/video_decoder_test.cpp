#include "media_fixture.hpp"

#include <edit_atlas/media/video_decoder.hpp>

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace edit_atlas::media {
namespace {

[[nodiscard]] std::filesystem::path
UniqueTemporaryPath(std::string_view extension) {
    std::random_device random;
    return std::filesystem::temp_directory_path() /
           ("edit-atlas-media-test-" +
            std::to_string(static_cast<unsigned long long>(random())) + "." +
            std::string{extension});
}

class TemporaryMediaFile final {
  public:
    explicit TemporaryMediaFile(std::string_view extension)
        : path_{UniqueTemporaryPath(extension)} {}

    ~TemporaryMediaFile(void) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove(path_, error));
    }

    TemporaryMediaFile(const TemporaryMediaFile &) = delete;
    TemporaryMediaFile &operator=(const TemporaryMediaFile &) = delete;
    TemporaryMediaFile(TemporaryMediaFile &&) = delete;
    TemporaryMediaFile &operator=(TemporaryMediaFile &&) = delete;

    [[nodiscard]] const std::filesystem::path &Path(void) const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

struct ContainerCase final {
    std::string_view extension;
    std::string_view muxer;
    std::string_view detected_name;
};

class VideoDecoderContainerTest : public testing::TestWithParam<ContainerCase> {
};

TEST_P(VideoDecoderContainerTest, OpensAndDecodesSupportedContainer) {
    const auto &container = GetParam();
    TemporaryMediaFile fixture{container.extension};
    const auto fixture_result = test::WriteVideoFixture(
        fixture.Path(), container.muxer, test::FixtureVideoCodec::kMpeg2Video);
    ASSERT_TRUE(fixture_result.has_value())
        << (fixture_result.has_value() ? "" : fixture_result.error());

    auto decoder_result = VideoDecoder::Open(fixture.Path());
    ASSERT_TRUE(decoder_result.has_value())
        << (decoder_result.has_value() ? "" : decoder_result.error().message);
    auto &decoder = **decoder_result;
    EXPECT_NE(
        decoder.Information().container_names.find(container.detected_name),
        std::string::npos);
    ASSERT_LT(decoder.Information().selected_video_stream,
              decoder.Information().streams.size());
    EXPECT_EQ(decoder.Information()
                  .streams[decoder.Information().selected_video_stream]
                  .codec_name,
              "mpeg2video");

    const auto frame_result = decoder.ReadFrame();
    ASSERT_TRUE(frame_result.has_value())
        << (frame_result.has_value() ? "" : frame_result.error().message);
    ASSERT_TRUE(frame_result->has_value());
    EXPECT_EQ((*frame_result)->width, 720);
    EXPECT_EQ((*frame_result)->height, 576);
    EXPECT_EQ((*frame_result)->row_stride, 2'160U);
    EXPECT_EQ((*frame_result)->pixels.size(), 1'244'160U);

    const auto seek_result = decoder.Seek(0);
    ASSERT_TRUE(seek_result.has_value())
        << (seek_result.has_value() ? "" : seek_result.error().message);
    const auto repeated_frame = decoder.ReadFrame();
    ASSERT_TRUE(repeated_frame.has_value())
        << (repeated_frame.has_value() ? "" : repeated_frame.error().message);
    EXPECT_TRUE(repeated_frame->has_value());
}

INSTANTIATE_TEST_SUITE_P(SupportedContainers, VideoDecoderContainerTest,
                         testing::Values(ContainerCase{"mov", "mov", "mov"},
                                         ContainerCase{"mp4", "mp4", "mov"},
                                         ContainerCase{"mxf", "mxf", "mxf"}));

TEST(VideoDecoderTest, ReturnsStructuredFailureForMissingInput) {
    const auto path =
        std::filesystem::temp_directory_path() / "edit-atlas-missing-video.mp4";
    std::error_code error;
    static_cast<void>(std::filesystem::remove(path, error));

    const auto result = VideoDecoder::Open(path);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().path, path);
    EXPECT_EQ(result.error().kind, VideoDecoderFailureKind::kOpenInput);
    EXPECT_LT(result.error().backend_error, 0);
    EXPECT_FALSE(result.error().message.empty());
}

TEST(VideoDecoderTest, ReturnsActionableFailureForUnsupportedCodec) {
    TemporaryMediaFile fixture{"mov"};
    const auto fixture_result = test::WriteVideoFixture(
        fixture.Path(), "mov", test::FixtureVideoCodec::kRawVideo);
    ASSERT_TRUE(fixture_result.has_value())
        << (fixture_result.has_value() ? "" : fixture_result.error());

    const auto result = VideoDecoder::Open(fixture.Path());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, VideoDecoderFailureKind::kUnsupportedCodec);
    EXPECT_EQ(result.error().codec_name, "rawvideo");
    EXPECT_NE(result.error().message.find("supported codecs"),
              std::string::npos);
}

TEST(FfmpegComplianceTest, UsesAnLgplBuildWithoutForbiddenFeatures) {
    const auto information = GetVideoBackendInformation();
    EXPECT_EQ(information.name, "FFmpeg");
    EXPECT_FALSE(information.version.empty());
    EXPECT_NE(information.license.find("LGPL"), std::string::npos);

    constexpr std::array<std::string_view, 4> kForbiddenOptions{
        "--enable-gpl", "--enable-nonfree", "--enable-libx264",
        "--enable-libx265"};
    for (const auto option : kForbiddenOptions) {
        EXPECT_EQ(information.configuration.find(option), std::string::npos)
            << option;
    }
}

} // namespace
} // namespace edit_atlas::media
