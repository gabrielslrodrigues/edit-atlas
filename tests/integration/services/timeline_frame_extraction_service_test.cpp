#include <edit_atlas/services/timeline_frame_extraction_service.hpp>

#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <edit_atlas/test/media_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <system_error>
#include <utility>

namespace edit_atlas::services {
namespace {

[[nodiscard]] std::filesystem::path UniqueTemporaryPath(void) {
    std::random_device random;
    return std::filesystem::temp_directory_path() /
           ("edit-atlas-frame-extraction-" +
            std::to_string(static_cast<unsigned long long>(random())) + ".mov");
}

class TemporaryMediaFile final {
  public:
    TemporaryMediaFile(void) : path_{UniqueTemporaryPath()} {}

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

[[nodiscard]] core::EditEvent Event(std::string identifier,
                                    std::int64_t record_frame,
                                    const core::FrameRate &rate) {
    auto source_start = core::Timecode::FromFrameCount(
                            0, rate, core::TimecodeMode::kNonDropFrame)
                            .value();
    auto source_end = core::Timecode::FromFrameCount(
                          1, rate, core::TimecodeMode::kNonDropFrame)
                          .value();
    auto record_start =
        core::Timecode::FromFrameCount(record_frame, rate,
                                       core::TimecodeMode::kNonDropFrame)
            .value();
    auto record_end =
        core::Timecode::FromFrameCount(record_frame + 1, rate,
                                       core::TimecodeMode::kNonDropFrame)
            .value();
    return core::EditEvent{
        .identifier = std::move(identifier),
        .reel = "AX",
        .track =
            core::Track{
                .kind = core::TrackKind::kVideo,
                .identifier = "V",
            },
        .edit_type = core::EditType::kCut,
        .transition = std::nullopt,
        .source_range = core::TimecodeRange::Create(std::move(source_start),
                                                    std::move(source_end))
                            .value(),
        .record_range = core::TimecodeRange::Create(std::move(record_start),
                                                    std::move(record_end))
                            .value(),
        .comments = {},
        .metadata = {},
        .provenance = std::nullopt,
    };
}

[[nodiscard]] core::TimelineDocument Timeline(const core::FrameRate &rate,
                                              std::int64_t record_start) {
    return core::TimelineDocument{
        .title = "Extraction integration fixture",
        .frame_rate = rate,
        .timecode_mode = core::TimecodeMode::kNonDropFrame,
        .events =
            {
                Event("001", record_start, rate),
                Event("002", record_start + 2, rate),
            },
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
}

struct FrameRateCase final {
    std::int32_t numerator;
    std::int32_t denominator;
    std::int64_t one_hour_frame;
};

class TimelineFrameExtractionIntegrationTest
    : public testing::TestWithParam<FrameRateCase> {};

TEST_P(TimelineFrameExtractionIntegrationTest,
       ExtractsExactFixtureFramesAcrossTimelineGap) {
    const auto &frame_rate = GetParam();
    TemporaryMediaFile fixture;
    auto fixture_options = media::test::VideoFixtureOptions{};
    fixture_options.frame_rate_numerator = frame_rate.numerator;
    fixture_options.frame_rate_denominator = frame_rate.denominator;
    fixture_options.starting_timecode = "01:00:00:00";
    const auto fixture_result =
        media::test::WriteVideoFixture(fixture.Path(), "mov", fixture_options);
    ASSERT_TRUE(fixture_result.has_value())
        << (fixture_result.has_value() ? "" : fixture_result.error());
    const auto rate =
        core::FrameRate::Create(frame_rate.numerator, frame_rate.denominator)
            .value();
    const auto timeline = Timeline(rate, frame_rate.one_hour_frame);
    const TimelineVideoInspectionService inspection_service;
    auto inspection = inspection_service.Inspect(fixture.Path(), timeline);
    ASSERT_TRUE(inspection.has_value())
        << (inspection.has_value()
                ? ""
                : inspection.error().diagnostics.front().message);
    const TimelineFrameExtractionService extraction_service;
    const TimelineFrameExtractionOptions extraction_options{
        .frame_output =
            media::VideoFrameOutputOptions{
                .size_limit =
                    media::VideoFrameSizeLimit{
                        .maximum_width = 320,
                        .maximum_height = 180,
                    },
            },
    };

    const auto result = extraction_service.Extract(
        timeline, std::move(*inspection), extraction_options);

    ASSERT_TRUE(result.has_value())
        << (result.has_value() ? ""
                               : result.error().diagnostics.front().message);
    ASSERT_EQ(result->frames.size(), 2);
    EXPECT_EQ(result->frames[0].video_frame_index, 0);
    EXPECT_EQ(result->frames[1].video_frame_index, 2);
    EXPECT_EQ(result->frames[0].image->width, 225);
    EXPECT_EQ(result->frames[0].image->height, 180);
    EXPECT_NE(result->frames[0].image->pixels, result->frames[1].image->pixels);
}

INSTANTIATE_TEST_SUITE_P(SupportedFrameRates,
                         TimelineFrameExtractionIntegrationTest,
                         testing::Values(FrameRateCase{24, 1, 86'400},
                                         FrameRateCase{25, 1, 90'000}));

} // namespace
} // namespace edit_atlas::services
