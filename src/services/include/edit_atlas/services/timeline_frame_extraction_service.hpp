#ifndef EDIT_ATLAS_SERVICES_TIMELINE_FRAME_EXTRACTION_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_FRAME_EXTRACTION_SERVICE_HPP_

#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/media/video_decoder.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace edit_atlas::services {

namespace timeline_frame_extraction_diagnostic_code {

/// Requested decoded-frame output dimensions are invalid.
inline constexpr std::string_view kInvalidOutputSize =
    "video.extraction.invalid_output_size";
/// An event maps outside the validated video duration.
inline constexpr std::string_view kInvalidMapping =
    "video.extraction.invalid_mapping";
/// Seeking to an event frame failed.
inline constexpr std::string_view kSeekFailed = "video.extraction.seek_failed";
/// Decoding an event frame failed.
inline constexpr std::string_view kDecodeFailed =
    "video.extraction.decode_failed";
/// The decoder could not produce the exact requested frame.
inline constexpr std::string_view kFrameUnavailable =
    "video.extraction.frame_unavailable";
/// A decoded frame contains an invalid RGB image.
inline constexpr std::string_view kInvalidFrame =
    "video.extraction.invalid_frame";
/// The caller cancelled extraction before it completed.
inline constexpr std::string_view kCancelled = "video.extraction.cancelled";
/// The caller's progress callback threw an exception.
inline constexpr std::string_view kProgressCallbackFailed =
    "video.extraction.progress_callback_failed";

} // namespace timeline_frame_extraction_diagnostic_code

/// Caller-selected output settings applied while frames are decoded.
struct TimelineFrameExtractionOptions final {
    /// Optional frame dimensions chosen by the consuming frontend or exporter.
    media::VideoFrameOutputOptions frame_output;
};

/// Associates one timeline event with its extracted initial frame.
struct TimelineEventFrame final {
    /// Zero-based event position in the source timeline.
    std::size_t event_index;
    /// Stable event identifier copied from the source timeline.
    std::string event_identifier;
    /// Absolute Record In frame count used for the mapping.
    std::int64_t record_frame;
    /// Zero-based frame index extracted from the rendered video.
    std::int64_t video_frame_index;
    /// Shared immutable RGB image; duplicate frame mappings share ownership.
    std::shared_ptr<const media::RgbImage> image;
};

/// Reports completed work after each unique frame is extracted.
struct TimelineFrameExtractionProgress final {
    /// Number of unique frames already extracted.
    std::size_t completed_unique_frames;
    /// Total number of unique frames required by the timeline.
    std::size_t total_unique_frames;
    /// Number of event frames already resolved.
    std::size_t completed_events;
    /// Total number of timeline events being processed.
    std::size_t total_events;
};

/// Receives synchronous extraction progress notifications.
using TimelineFrameExtractionProgressCallback =
    std::function<void(const TimelineFrameExtractionProgress &)>;

/// Complete all-or-nothing initial-frame extraction output.
struct TimelineFrameExtractionReceipt final {
    /// Extracted event frames in original timeline order.
    std::vector<TimelineEventFrame> frames;
    /// Number of distinct video frames decoded for the events.
    std::size_t unique_frame_count;
};

/// Structured failure returned without partial frame output.
struct TimelineFrameExtractionFailure final {
    /// Decoder failure when seeking or reading media failed.
    std::optional<media::VideoDecoderFailure> decoder_failure;
    /// Diagnostics explaining why extraction did not complete.
    std::vector<core::Diagnostic> diagnostics;
};

/// Result of extracting every event's initial rendered-video frame.
using TimelineFrameExtractionResult =
    std::expected<TimelineFrameExtractionReceipt,
                  TimelineFrameExtractionFailure>;

/// Extracts initial event frames from validated rendered videos.
class TimelineFrameExtractionService final {
  public:
    /// Constructs a stateless extraction service.
    TimelineFrameExtractionService(void) = default;

    /// Consumes a validated decoder and extracts every event's Record In frame.
    ///
    /// Output sizing is selected by the caller; thumbnail presentation remains
    /// the responsibility of the consuming frontend or exporter.
    /// Progress callbacks execute synchronously and must not block. Exceptions
    /// are converted to structured failures. Cancellation and all failures
    /// discard partial output and release the consumed decoder.
    [[nodiscard]] TimelineFrameExtractionResult
    Extract(const core::TimelineDocument &timeline,
            TimelineVideoInspectionReceipt inspection,
            const TimelineFrameExtractionOptions &options = {},
            std::stop_token stop_token = {},
            const TimelineFrameExtractionProgressCallback &progress = {}) const;
};

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_TIMELINE_FRAME_EXTRACTION_SERVICE_HPP_
