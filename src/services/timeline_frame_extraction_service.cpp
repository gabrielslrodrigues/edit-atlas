#include <edit_atlas/services/timeline_frame_extraction_service.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::services {
namespace {

[[nodiscard]] core::Diagnostic Error(std::string_view code,
                                     std::string message) {
    return core::Diagnostic{
        .severity = core::DiagnosticSeverity::kError,
        .code = std::string{code},
        .message = std::move(message),
        .location = std::nullopt,
    };
}

[[nodiscard]] TimelineFrameExtractionFailure Failure(
    std::string_view code, std::string message,
    std::optional<media::VideoDecoderFailure> decoder_failure = std::nullopt) {
    std::vector<core::Diagnostic> diagnostics;
    diagnostics.push_back(Error(code, std::move(message)));
    return TimelineFrameExtractionFailure{
        .decoder_failure = std::move(decoder_failure),
        .diagnostics = std::move(diagnostics),
    };
}

[[nodiscard]] bool
ValidOptions(const TimelineFrameExtractionOptions &options) noexcept {
    return !options.frame_output.size_limit.has_value() ||
           (options.frame_output.size_limit->maximum_width > 0 &&
            options.frame_output.size_limit->maximum_height > 0);
}

[[nodiscard]] std::optional<std::int64_t>
AddFrames(std::int64_t frame, std::int64_t offset) noexcept {
    if ((offset > 0 &&
         frame > std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 &&
         frame < std::numeric_limits<std::int64_t>::min() - offset)) {
        return std::nullopt;
    }
    return frame + offset;
}

[[nodiscard]] bool ValidMapping(const TimelineVideoMapping &mapping) noexcept {
    const auto mapped_start = AddFrames(mapping.record_start_frame,
                                        mapping.record_to_video_frame_offset);
    return mapping.record_start_frame < mapping.record_end_frame_exclusive &&
           mapping.video_duration_frames > 0 && mapped_start.has_value() &&
           *mapped_start == 0;
}

[[nodiscard]] bool
ValidImage(const media::RgbImage &image,
           const TimelineFrameExtractionOptions &options) noexcept {
    if (image.width <= 0 || image.height <= 0 ||
        image.width > std::numeric_limits<std::int32_t>::max() / 3) {
        return false;
    }
    const auto minimum_stride = static_cast<std::size_t>(image.width) * 3U;
    if (image.row_stride < minimum_stride ||
        static_cast<std::size_t>(image.height) >
            std::numeric_limits<std::size_t>::max() / image.row_stride ||
        image.pixels.size() <
            image.row_stride * static_cast<std::size_t>(image.height)) {
        return false;
    }
    return !options.frame_output.size_limit.has_value() ||
           (image.width <= options.frame_output.size_limit->maximum_width &&
            image.height <= options.frame_output.size_limit->maximum_height);
}

[[nodiscard]] std::optional<TimelineFrameExtractionFailure>
ReportProgress(const TimelineFrameExtractionProgressCallback &callback,
               const TimelineFrameExtractionProgress &progress) {
    if (!callback) {
        return std::nullopt;
    }
    try {
        callback(progress);
    } catch (...) {
        return Failure(
            timeline_frame_extraction_diagnostic_code::kProgressCallbackFailed,
            "The extraction progress callback failed.");
    }
    return std::nullopt;
}

} // namespace

TimelineFrameExtractionResult TimelineFrameExtractionService::Extract(
    const core::TimelineDocument &timeline,
    TimelineVideoInspectionReceipt inspection,
    const TimelineFrameExtractionOptions &options, std::stop_token stop_token,
    const TimelineFrameExtractionProgressCallback &progress) const {
    if (!ValidOptions(options)) {
        return std::unexpected(Failure(
            timeline_frame_extraction_diagnostic_code::kInvalidOutputSize,
            "Decoded-frame output dimensions must be positive."));
    }
    if (inspection.decoder == nullptr || !ValidMapping(inspection.mapping)) {
        return std::unexpected(
            Failure(timeline_frame_extraction_diagnostic_code::kInvalidMapping,
                    "The validated video receipt is incomplete or invalid."));
    }

    std::map<std::int64_t, std::vector<std::size_t>> events_by_frame;
    for (std::size_t event_index = 0; event_index < timeline.events.size();
         ++event_index) {
        const auto record_frame =
            timeline.events[event_index].record_range.start().ToFrameCount();
        const auto record_end_frame = timeline.events[event_index]
                                          .record_range.end_exclusive()
                                          .ToFrameCount();
        const auto video_frame = AddFrames(
            record_frame, inspection.mapping.record_to_video_frame_offset);
        if (record_frame < inspection.mapping.record_start_frame ||
            record_end_frame > inspection.mapping.record_end_frame_exclusive ||
            !video_frame.has_value() || *video_frame < 0 ||
            *video_frame >= inspection.mapping.video_duration_frames) {
            return std::unexpected(Failure(
                timeline_frame_extraction_diagnostic_code::kInvalidMapping,
                "A timeline event is outside the validated record-to-video "
                "mapping."));
        }
        events_by_frame[*video_frame].push_back(event_index);
    }

    TimelineFrameExtractionProgress extraction_progress{
        .completed_unique_frames = 0,
        .total_unique_frames = events_by_frame.size(),
        .completed_events = 0,
        .total_events = timeline.events.size(),
    };
    if (const auto failure = ReportProgress(progress, extraction_progress);
        failure.has_value()) {
        return std::unexpected(std::move(*failure));
    }

    std::vector<TimelineEventFrame> frames;
    frames.reserve(timeline.events.size());
    for (const auto &[video_frame, event_indices] : events_by_frame) {
        if (stop_token.stop_requested()) {
            return std::unexpected(
                Failure(timeline_frame_extraction_diagnostic_code::kCancelled,
                        "Initial-frame extraction was cancelled."));
        }

        auto seek_result = inspection.decoder->SeekToFrame(video_frame);
        if (!seek_result.has_value()) {
            auto decoder_failure = std::move(seek_result.error());
            return std::unexpected(
                Failure(timeline_frame_extraction_diagnostic_code::kSeekFailed,
                        decoder_failure.message, std::move(decoder_failure)));
        }

        std::optional<media::DecodedVideoFrame> exact_frame;
        while (!exact_frame.has_value()) {
            if (stop_token.stop_requested()) {
                return std::unexpected(Failure(
                    timeline_frame_extraction_diagnostic_code::kCancelled,
                    "Initial-frame extraction was cancelled."));
            }
            auto frame_result =
                inspection.decoder->ReadFrame(options.frame_output);
            if (!frame_result.has_value()) {
                auto decoder_failure = std::move(frame_result.error());
                return std::unexpected(Failure(
                    timeline_frame_extraction_diagnostic_code::kDecodeFailed,
                    decoder_failure.message, std::move(decoder_failure)));
            }
            if (!frame_result->has_value()) {
                return std::unexpected(Failure(
                    timeline_frame_extraction_diagnostic_code::
                        kFrameUnavailable,
                    "The video ended before the requested event frame was "
                    "decoded."));
            }
            auto frame = std::move(**frame_result);
            if (!frame.frame_index.has_value()) {
                return std::unexpected(Failure(
                    timeline_frame_extraction_diagnostic_code::
                        kFrameUnavailable,
                    "A decoded frame has no usable presentation index."));
            }
            if (*frame.frame_index < video_frame) {
                continue;
            }
            if (*frame.frame_index > video_frame) {
                return std::unexpected(Failure(
                    timeline_frame_extraction_diagnostic_code::
                        kFrameUnavailable,
                    "The decoder passed the requested event frame without "
                    "producing it."));
            }
            exact_frame = std::move(frame);
        }

        if (!ValidImage(exact_frame->image, options)) {
            return std::unexpected(Failure(
                timeline_frame_extraction_diagnostic_code::kInvalidFrame,
                "The decoded RGB frame image is invalid or exceeds the "
                "requested output dimensions."));
        }
        auto image = std::make_shared<const media::RgbImage>(
            std::move(exact_frame->image));
        for (const auto event_index : event_indices) {
            const auto &event = timeline.events[event_index];
            frames.push_back(TimelineEventFrame{
                .event_index = event_index,
                .event_identifier = event.identifier,
                .record_frame = event.record_range.start().ToFrameCount(),
                .video_frame_index = video_frame,
                .image = image,
            });
        }

        ++extraction_progress.completed_unique_frames;
        extraction_progress.completed_events += event_indices.size();
        if (const auto failure = ReportProgress(progress, extraction_progress);
            failure.has_value()) {
            return std::unexpected(std::move(*failure));
        }
    }

    std::ranges::sort(frames, {}, &TimelineEventFrame::event_index);
    return TimelineFrameExtractionReceipt{
        .frames = std::move(frames),
        .unique_frame_count = events_by_frame.size(),
    };
}

} // namespace edit_atlas::services
