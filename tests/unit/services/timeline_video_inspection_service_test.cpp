#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <edit_atlas/media/video_decoder.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::services {
namespace {

[[nodiscard]] core::FrameRate Rate(std::int64_t numerator = 25,
                                   std::int64_t denominator = 1) {
    return core::FrameRate::Create(numerator, denominator).value();
}

[[nodiscard]] core::TimelineDocument
Timeline(const core::FrameRate &rate = Rate(),
         core::TimecodeMode mode = core::TimecodeMode::kNonDropFrame,
         std::int64_t start_frame = 90'000, std::int64_t duration = 3) {
    auto start =
        core::Timecode::FromFrameCount(start_frame, rate, mode).value();
    auto end =
        core::Timecode::FromFrameCount(start_frame + duration, rate, mode)
            .value();
    auto source_start = core::Timecode::FromFrameCount(0, rate, mode).value();
    auto source_end =
        core::Timecode::FromFrameCount(duration, rate, mode).value();
    return core::TimelineDocument{
        .title = "Inspection fixture",
        .frame_rate = rate,
        .timecode_mode = mode,
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
                    .record_range = core::TimecodeRange::Create(
                                        std::move(start), std::move(end))
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

[[nodiscard]] media::VideoMediaInfo
MediaInformation(std::string timecode = "01:00:00:00",
                 media::Rational average_rate = {25, 1},
                 media::Rational nominal_rate = {25, 1},
                 std::optional<std::int64_t> frame_count = 3) {
    std::vector<media::MediaMetadataEntry> metadata;
    if (!timecode.empty()) {
        metadata.push_back(media::MediaMetadataEntry{
            .key = "timecode",
            .value = std::move(timecode),
        });
    }
    return media::VideoMediaInfo{
        .path = std::filesystem::path{"fixture.mov"},
        .container = media::MediaContainer::kMov,
        .container_names = "mov,mp4,m4a,3gp,3g2,mj2",
        .container_long_name = "QuickTime / MOV",
        .duration_microseconds = std::nullopt,
        .start_time_microseconds = 0,
        .metadata = {},
        .streams =
            {
                media::MediaStreamInfo{
                    .index = 0,
                    .type = media::MediaStreamType::kVideo,
                    .codec_name = "mpeg2video",
                    .codec_long_name = "MPEG-2 video",
                    .time_base = media::Rational{1, 25},
                    .average_frame_rate = average_rate,
                    .nominal_frame_rate = nominal_rate,
                    .duration = frame_count,
                    .start_time = 0,
                    .frame_count = frame_count,
                    .width = 720,
                    .height = 576,
                    .metadata = std::move(metadata),
                },
            },
        .selected_video_stream = 0,
    };
}

void ExpectOnlyDiagnostic(const TimelineVideoValidationResult &result,
                          std::string_view code) {
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().diagnostics.size(), 1);
    EXPECT_EQ(result.error().diagnostics.front().code, code);
}

TEST(TimelineVideoInspectionServiceTest, AcceptsMatchingConstantRateMedia) {
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(MediaInformation(), Timeline());

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->video_start_timecode.ToFrameCount(), 90'000);
    EXPECT_EQ(result->record_to_video_frame_offset, -90'000);
    EXPECT_EQ(result->record_start_frame, 90'000);
    EXPECT_EQ(result->record_end_frame_exclusive, 90'003);
    EXPECT_EQ(result->video_duration_frames, 3);
}

TEST(TimelineVideoInspectionServiceTest, AcceptsOneFrameDurationTolerance) {
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(
        MediaInformation("01:00:00:00", {25, 1}, {25, 1}, 4), Timeline());

    EXPECT_TRUE(result.has_value());
}

TEST(TimelineVideoInspectionServiceTest, RejectsUnsupportedContainer) {
    auto media_information = MediaInformation();
    media_information.container = media::MediaContainer::kOther;
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(media_information, Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kUnsupportedContainer);
}

TEST(TimelineVideoInspectionServiceTest, RejectsMissingEmbeddedTimecode) {
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(MediaInformation(""), Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kMissingTimecode);
}

TEST(TimelineVideoInspectionServiceTest, RejectsInvalidEmbeddedTimecode) {
    const TimelineVideoInspectionService service;

    const auto result =
        service.Validate(MediaInformation("not-timecode"), Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kInvalidTimecode);
}

TEST(TimelineVideoInspectionServiceTest, RejectsVariableFrameRateMetadata) {
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(
        MediaInformation("01:00:00:00", {25, 1}, {24, 1}), Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kVariableFrameRate);
}

TEST(TimelineVideoInspectionServiceTest, RejectsMissingFrameRateMetadata) {
    auto media_information = MediaInformation();
    media_information.streams.front().average_frame_rate = std::nullopt;
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(media_information, Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kMissingFrameRate);
}

TEST(TimelineVideoInspectionServiceTest, RejectsFrameRateMismatch) {
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(
        MediaInformation("01:00:00:00", {24, 1}, {24, 1}), Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kFrameRateMismatch);
}

TEST(TimelineVideoInspectionServiceTest, RejectsTimecodeModeMismatch) {
    const auto rate = Rate(30'000, 1'001);
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(
        MediaInformation("01:00:00:00", {30'000, 1'001}, {30'000, 1'001}),
        Timeline(rate, core::TimecodeMode::kDropFrame, 107'892));

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kTimecodeModeMismatch);
}

TEST(TimelineVideoInspectionServiceTest, RejectsTimelineStartMismatch) {
    const TimelineVideoInspectionService service;

    const auto result =
        service.Validate(MediaInformation("00:59:59:24"), Timeline());

    ExpectOnlyDiagnostic(
        result, timeline_video_diagnostic_code::kTimelineStartMismatch);
}

TEST(TimelineVideoInspectionServiceTest, RejectsConflictingTimecodes) {
    auto media_information = MediaInformation();
    media_information.metadata.push_back(media::MediaMetadataEntry{
        .key = "timecode",
        .value = "02:00:00:00",
    });
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(media_information, Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kInvalidTimecode);
}

TEST(TimelineVideoInspectionServiceTest, RejectsDurationOutsideTolerance) {
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(
        MediaInformation("01:00:00:00", {25, 1}, {25, 1}, 5), Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kDurationMismatch);
}

TEST(TimelineVideoInspectionServiceTest, RejectsMissingDuration) {
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(
        MediaInformation("01:00:00:00", {25, 1}, {25, 1}, std::nullopt),
        Timeline());

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kMissingDuration);
}

TEST(TimelineVideoInspectionServiceTest, RejectsEmptyTimeline) {
    auto timeline = Timeline();
    timeline.events.clear();
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(MediaInformation(), timeline);

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kEmptyTimeline);
}

TEST(TimelineVideoInspectionServiceTest, RejectsInconsistentTimelineRates) {
    auto timeline = Timeline();
    timeline.frame_rate = Rate(24, 1);
    const TimelineVideoInspectionService service;

    const auto result = service.Validate(MediaInformation(), timeline);

    ExpectOnlyDiagnostic(result,
                         timeline_video_diagnostic_code::kInconsistentTimeline);
}

} // namespace
} // namespace edit_atlas::services
