#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <edit_atlas/test/media_fixture.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace edit_atlas::services {
namespace {

[[nodiscard]] std::filesystem::path
UniqueTemporaryPath(std::string_view extension) {
    std::random_device random;
    return std::filesystem::temp_directory_path() /
           ("edit-atlas-video-inspection-" +
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

[[nodiscard]] core::TimelineDocument Timeline(void) {
    const auto rate = core::FrameRate::Create(25, 1).value();
    auto source_start = core::Timecode::FromFrameCount(
                            0, rate, core::TimecodeMode::kNonDropFrame)
                            .value();
    auto source_end = core::Timecode::FromFrameCount(
                          3, rate, core::TimecodeMode::kNonDropFrame)
                          .value();
    auto record_start = core::Timecode::FromFrameCount(
                            90'000, rate, core::TimecodeMode::kNonDropFrame)
                            .value();
    auto record_end = core::Timecode::FromFrameCount(
                          90'003, rate, core::TimecodeMode::kNonDropFrame)
                          .value();
    return core::TimelineDocument{
        .title = "Inspection integration fixture",
        .frame_rate = rate,
        .timecode_mode = core::TimecodeMode::kNonDropFrame,
        .events =
            {
                core::EditEvent{
                    .identifier = "001",
                    .reel = "AX",
                    .track =
                        core::Track{
                            .kind = core::TrackKind::kVideo,
                            .identifier = "V",
                        },
                    .edit_type = core::EditType::kCut,
                    .transition = std::nullopt,
                    .source_range =
                        core::TimecodeRange::Create(std::move(source_start),
                                                    std::move(source_end))
                            .value(),
                    .record_range =
                        core::TimecodeRange::Create(std::move(record_start),
                                                    std::move(record_end))
                            .value(),
                    .comments = {},
                    .metadata = {},
                    .provenance = std::nullopt,
                },
            },
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
}

struct ContainerCase final {
    std::string_view extension;
    std::string_view muxer;
};

class TimelineVideoContainerInspectionTest
    : public testing::TestWithParam<ContainerCase> {};

TEST_P(TimelineVideoContainerInspectionTest,
       AcceptsMatchingVideoWithEmbeddedTimecode) {
    const auto &container = GetParam();
    TemporaryMediaFile fixture{container.extension};
    auto options = media::test::VideoFixtureOptions{};
    options.starting_timecode = "01:00:00:00";
    const auto fixture_result = media::test::WriteVideoFixture(
        fixture.Path(), container.muxer, options);
    ASSERT_TRUE(fixture_result.has_value())
        << (fixture_result.has_value() ? "" : fixture_result.error());
    const TimelineVideoInspectionService service;

    auto result = service.Inspect(fixture.Path(), Timeline());

    ASSERT_TRUE(result.has_value())
        << (result.has_value() ? ""
                               : result.error().diagnostics.front().message);
    EXPECT_NE(result->decoder, nullptr);
    EXPECT_EQ(result->mapping.video_start_timecode.ToFrameCount(), 90'000);
    EXPECT_EQ(result->mapping.record_to_video_frame_offset, -90'000);
    EXPECT_EQ(result->mapping.video_duration_frames, 3);
}

INSTANTIATE_TEST_SUITE_P(SupportedContainers,
                         TimelineVideoContainerInspectionTest,
                         testing::Values(ContainerCase{"mov", "mov"},
                                         ContainerCase{"mp4", "mp4"},
                                         ContainerCase{"mxf", "mxf"}));

TEST(TimelineVideoInspectionServiceIntegrationTest,
     RejectsVideoWithoutEmbeddedTimecode) {
    TemporaryMediaFile fixture{"mov"};
    const auto fixture_result = media::test::WriteVideoFixture(
        fixture.Path(), "mov", media::test::VideoFixtureOptions{});
    ASSERT_TRUE(fixture_result.has_value())
        << (fixture_result.has_value() ? "" : fixture_result.error());
    const TimelineVideoInspectionService service;

    const auto result = service.Inspect(fixture.Path(), Timeline());

    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().diagnostics.size(), 1);
    EXPECT_EQ(result.error().diagnostics.front().code,
              timeline_video_diagnostic_code::kMissingTimecode);
}

TEST(TimelineVideoInspectionServiceIntegrationTest,
     PreservesStructuredDecoderFailure) {
    const auto path = UniqueTemporaryPath("mov");
    std::error_code error;
    static_cast<void>(std::filesystem::remove(path, error));
    const TimelineVideoInspectionService service;

    const auto result = service.Inspect(path, Timeline());

    ASSERT_FALSE(result.has_value());
    ASSERT_TRUE(result.error().decoder_failure.has_value());
    EXPECT_EQ(result.error().decoder_failure->kind,
              media::VideoDecoderFailureKind::kOpenInput);
    ASSERT_EQ(result.error().diagnostics.size(), 1);
    EXPECT_EQ(result.error().diagnostics.front().code,
              timeline_video_diagnostic_code::kOpenFailed);
}

} // namespace
} // namespace edit_atlas::services
