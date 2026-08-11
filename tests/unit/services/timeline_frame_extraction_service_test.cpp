#include <edit_atlas/services/timeline_frame_extraction_service.hpp>

#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/rgb_image.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <edit_atlas/media/video_decoder.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::services {
namespace {

struct FakeDecoderState final {
    std::vector<std::int64_t> seek_requests;
    std::size_t read_count = 0;
    std::size_t destruction_count = 0;
    bool fail_seek = false;
    bool fail_read = false;
    std::optional<std::int64_t> skipped_frame;
};

[[nodiscard]] media::VideoMediaInfo FakeMediaInformation(void) {
    return media::VideoMediaInfo{
        .path = std::filesystem::path{"fixture.mov"},
        .container = media::MediaContainer::kMov,
        .container_names = "mov,mp4,m4a,3gp,3g2,mj2",
        .container_long_name = "QuickTime / MOV",
        .duration_microseconds = 400'000,
        .start_time_microseconds = 0,
        .metadata = {},
        .streams =
            {
                media::MediaStreamInfo{
                    .index = 0,
                    .type = media::MediaStreamType::kVideo,
                    .codec_name = "fake",
                    .codec_long_name = "Deterministic fake video",
                    .time_base = media::Rational{1, 25},
                    .average_frame_rate = media::Rational{25, 1},
                    .nominal_frame_rate = media::Rational{25, 1},
                    .duration = 10,
                    .start_time = 0,
                    .frame_count = 10,
                    .width = 4,
                    .height = 2,
                    .metadata = {},
                },
            },
        .selected_video_stream = 0,
    };
}

[[nodiscard]] media::DecodedVideoFrame
Frame(std::int64_t frame_index, const media::VideoFrameOutputOptions &options) {
    auto width = std::int32_t{4};
    auto height = std::int32_t{2};
    if (options.size_limit.has_value() &&
        (width > options.size_limit->maximum_width ||
         height > options.size_limit->maximum_height)) {
        const auto width_limited =
            static_cast<std::int64_t>(options.size_limit->maximum_width) *
                height <=
            static_cast<std::int64_t>(options.size_limit->maximum_height) *
                width;
        if (width_limited) {
            height = std::max<std::int32_t>(
                1, static_cast<std::int32_t>(static_cast<std::int64_t>(height) *
                                             options.size_limit->maximum_width /
                                             width));
            width = options.size_limit->maximum_width;
        } else {
            width = std::max<std::int32_t>(
                1, static_cast<std::int32_t>(
                       static_cast<std::int64_t>(width) *
                       options.size_limit->maximum_height / height));
            height = options.size_limit->maximum_height;
        }
    }
    const auto row_stride = static_cast<std::size_t>(width) * 3U;
    return media::DecodedVideoFrame{
        .frame_index = frame_index,
        .presentation_timestamp = frame_index,
        .image =
            core::RgbImage{
                .width = width,
                .height = height,
                .row_stride = row_stride,
                .pixels = std::vector<std::byte>(
                    row_stride * static_cast<std::size_t>(height),
                    static_cast<std::byte>(frame_index)),
            },
        .key_frame = frame_index % 2 == 0,
    };
}

class FakeVideoDecoder final : public media::VideoDecoder {
  public:
    explicit FakeVideoDecoder(std::shared_ptr<FakeDecoderState> state)
        : state_{std::move(state)}, information_{FakeMediaInformation()} {}

    ~FakeVideoDecoder(void) override { ++state_->destruction_count; }

    [[nodiscard]] const media::VideoMediaInfo &
    Information(void) const noexcept override {
        return information_;
    }

    [[nodiscard]] std::expected<void, media::VideoDecoderFailure>
    Seek(std::int64_t timestamp) override {
        return SeekToFrame(timestamp);
    }

    [[nodiscard]] std::expected<void, media::VideoDecoderFailure>
    SeekToFrame(std::int64_t frame_index) override {
        state_->seek_requests.push_back(frame_index);
        if (state_->fail_seek) {
            return std::unexpected(media::VideoDecoderFailure{
                .path = information_.path,
                .kind = media::VideoDecoderFailureKind::kSeek,
                .backend_error = -1,
                .codec_name = "fake",
                .message = "fake seek failure",
            });
        }
        next_frame_ = frame_index == 0 ? 0 : frame_index - 1;
        return {};
    }

    [[nodiscard]] std::expected<std::optional<media::DecodedVideoFrame>,
                                media::VideoDecoderFailure>
    ReadFrame(const media::VideoFrameOutputOptions &options) override {
        ++state_->read_count;
        if (state_->fail_read) {
            return std::unexpected(media::VideoDecoderFailure{
                .path = information_.path,
                .kind = media::VideoDecoderFailureKind::kDecodeFrame,
                .backend_error = -2,
                .codec_name = "fake",
                .message = "fake decode failure",
            });
        }
        if (state_->skipped_frame == next_frame_) {
            ++next_frame_;
        }
        if (next_frame_ >= 10) {
            return std::optional<media::DecodedVideoFrame>{};
        }
        return std::optional<media::DecodedVideoFrame>{
            Frame(next_frame_++, options)};
    }

  private:
    std::shared_ptr<FakeDecoderState> state_;
    media::VideoMediaInfo information_;
    std::int64_t next_frame_ = 0;
};

[[nodiscard]] core::FrameRate Rate(void) {
    return core::FrameRate::Create(25, 1).value();
}

[[nodiscard]] core::EditEvent Event(std::string identifier,
                                    std::int64_t record_frame) {
    const auto rate = Rate();
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

[[nodiscard]] core::TimelineDocument Timeline(void) {
    return core::TimelineDocument{
        .title = "Extraction fixture",
        .frame_rate = Rate(),
        .timecode_mode = core::TimecodeMode::kNonDropFrame,
        .events =
            {
                Event("001", 90'000),
                Event("002", 90'005),
                Event("003", 90'000),
            },
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
}

[[nodiscard]] TimelineVideoInspectionReceipt
Inspection(const std::shared_ptr<FakeDecoderState> &state) {
    const auto rate = Rate();
    return TimelineVideoInspectionReceipt{
        .decoder = std::make_unique<FakeVideoDecoder>(state),
        .mapping =
            TimelineVideoMapping{
                .video_start_timecode =
                    core::Timecode::FromFrameCount(
                        90'000, rate, core::TimecodeMode::kNonDropFrame)
                        .value(),
                .record_to_video_frame_offset = -90'000,
                .record_start_frame = 90'000,
                .record_end_frame_exclusive = 90'010,
                .video_duration_frames = 10,
            },
    };
}

void ExpectOnlyDiagnostic(const TimelineFrameExtractionResult &result,
                          std::string_view code) {
    ASSERT_FALSE(result.has_value());
    ASSERT_EQ(result.error().diagnostics.size(), 1);
    EXPECT_EQ(result.error().diagnostics.front().code, code);
}

TEST(TimelineFrameExtractionServiceTest,
     ExtractsExactFramesAcrossGapsAndDeduplicatesImages) {
    const auto state = std::make_shared<FakeDecoderState>();
    std::vector<TimelineFrameExtractionProgress> progress;
    const TimelineFrameExtractionService service;
    const TimelineFrameExtractionOptions options{
        .frame_output =
            media::VideoFrameOutputOptions{
                .size_limit =
                    media::VideoFrameSizeLimit{
                        .maximum_width = 2,
                        .maximum_height = 2,
                    },
            },
    };

    const auto result = service.Extract(
        Timeline(), Inspection(state), options, {},
        [&progress](const TimelineFrameExtractionProgress &update) {
            progress.push_back(update);
        });

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->frames.size(), 3);
    EXPECT_EQ(result->unique_frame_count, 2);
    EXPECT_EQ(state->seek_requests, (std::vector<std::int64_t>{0, 5}));
    EXPECT_EQ(result->frames[0].video_frame_index, 0);
    EXPECT_EQ(result->frames[1].video_frame_index, 5);
    EXPECT_EQ(result->frames[2].video_frame_index, 0);
    EXPECT_EQ(result->frames[0].image, result->frames[2].image);
    EXPECT_NE(result->frames[0].image, result->frames[1].image);
    EXPECT_EQ(result->frames[0].image->width, 2);
    EXPECT_EQ(result->frames[0].image->height, 1);
    ASSERT_FALSE(result->frames[0].image->pixels.empty());
    ASSERT_FALSE(result->frames[1].image->pixels.empty());
    EXPECT_EQ(result->frames[0].image->pixels.front(), std::byte{0});
    EXPECT_EQ(result->frames[1].image->pixels.front(), std::byte{5});
    ASSERT_EQ(progress.size(), 3);
    EXPECT_EQ(progress.back().completed_unique_frames, 2);
    EXPECT_EQ(progress.back().completed_events, 3);
    EXPECT_EQ(state->destruction_count, 1);
}

TEST(TimelineFrameExtractionServiceTest, CancelsWithoutReturningPartialOutput) {
    const auto state = std::make_shared<FakeDecoderState>();
    std::stop_source stop_source;
    const TimelineFrameExtractionService service;

    const auto result = service.Extract(
        Timeline(), Inspection(state), {}, stop_source.get_token(),
        [&stop_source](const TimelineFrameExtractionProgress &progress) {
            if (progress.completed_unique_frames == 1) {
                stop_source.request_stop();
            }
        });

    ExpectOnlyDiagnostic(result,
                         timeline_frame_extraction_diagnostic_code::kCancelled);
    EXPECT_EQ(state->destruction_count, 1);
}

TEST(TimelineFrameExtractionServiceTest, RejectsMissingExactFrame) {
    const auto state = std::make_shared<FakeDecoderState>();
    state->skipped_frame = 5;
    const TimelineFrameExtractionService service;

    const auto result = service.Extract(Timeline(), Inspection(state));

    ExpectOnlyDiagnostic(
        result, timeline_frame_extraction_diagnostic_code::kFrameUnavailable);
}

TEST(TimelineFrameExtractionServiceTest, PreservesDecoderSeekFailure) {
    const auto state = std::make_shared<FakeDecoderState>();
    state->fail_seek = true;
    const TimelineFrameExtractionService service;

    const auto result = service.Extract(Timeline(), Inspection(state));

    ExpectOnlyDiagnostic(
        result, timeline_frame_extraction_diagnostic_code::kSeekFailed);
    ASSERT_TRUE(result.error().decoder_failure.has_value());
    EXPECT_EQ(result.error().decoder_failure->kind,
              media::VideoDecoderFailureKind::kSeek);
}

TEST(TimelineFrameExtractionServiceTest, PreservesDecoderReadFailure) {
    const auto state = std::make_shared<FakeDecoderState>();
    state->fail_read = true;
    const TimelineFrameExtractionService service;

    const auto result = service.Extract(Timeline(), Inspection(state));

    ExpectOnlyDiagnostic(
        result, timeline_frame_extraction_diagnostic_code::kDecodeFailed);
    ASSERT_TRUE(result.error().decoder_failure.has_value());
    EXPECT_EQ(result.error().decoder_failure->kind,
              media::VideoDecoderFailureKind::kDecodeFrame);
}

TEST(TimelineFrameExtractionServiceTest, RejectsThrowingProgressCallback) {
    const auto state = std::make_shared<FakeDecoderState>();
    const TimelineFrameExtractionService service;

    const auto result = service.Extract(
        Timeline(), Inspection(state), {}, {},
        [](const TimelineFrameExtractionProgress &) { throw 1; });

    ExpectOnlyDiagnostic(
        result,
        timeline_frame_extraction_diagnostic_code::kProgressCallbackFailed);
    EXPECT_TRUE(state->seek_requests.empty());
}

TEST(TimelineFrameExtractionServiceTest, RejectsInvalidOutputDimensions) {
    const auto state = std::make_shared<FakeDecoderState>();
    const TimelineFrameExtractionService service;
    const TimelineFrameExtractionOptions options{
        .frame_output =
            media::VideoFrameOutputOptions{
                .size_limit =
                    media::VideoFrameSizeLimit{
                        .maximum_width = 0,
                        .maximum_height = 180,
                    },
            },
    };

    const auto result = service.Extract(Timeline(), Inspection(state), options);

    ExpectOnlyDiagnostic(
        result, timeline_frame_extraction_diagnostic_code::kInvalidOutputSize);
}

TEST(TimelineFrameExtractionServiceTest,
     RejectsTimelineOutsideValidatedMapping) {
    const auto state = std::make_shared<FakeDecoderState>();
    auto inspection = Inspection(state);
    inspection.mapping.record_end_frame_exclusive = 90'004;
    const TimelineFrameExtractionService service;

    const auto result = service.Extract(Timeline(), std::move(inspection));

    ExpectOnlyDiagnostic(
        result, timeline_frame_extraction_diagnostic_code::kInvalidMapping);
    EXPECT_TRUE(state->seek_requests.empty());
}

} // namespace
} // namespace edit_atlas::services
