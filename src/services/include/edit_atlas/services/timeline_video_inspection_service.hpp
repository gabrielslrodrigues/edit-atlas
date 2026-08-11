#ifndef EDIT_ATLAS_SERVICES_TIMELINE_VIDEO_INSPECTION_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_VIDEO_INSPECTION_SERVICE_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/media/video_decoder.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace edit_atlas::services {

/// Maximum accepted difference between the video and record-timeline duration.
inline constexpr std::int64_t kVideoDurationToleranceFrames = 1;

/// The rendered-video requirements shared by all user-facing frontends.
inline constexpr std::string_view kRenderedVideoInputContract =
    "Select a constant-frame-rate MOV, MP4, or MXF render whose embedded "
    "starting timecode, frame rate, and duration match the imported EDL "
    "record timeline. Videos without usable embedded timecode cannot be used.";

/// Stable diagnostic identifiers produced by timeline video inspection.
namespace timeline_video_diagnostic_code {

/// The candidate path could not be opened as supported video media.
inline constexpr std::string_view kOpenFailed = "video.open_failed";
/// The inspected media is not a supported MOV, MP4, or MXF container.
inline constexpr std::string_view kUnsupportedContainer =
    "video.unsupported_container";
/// The imported timeline has no events to map to video frames.
inline constexpr std::string_view kEmptyTimeline = "video.empty_timeline";
/// Record ranges disagree with the timeline's primary timing properties.
inline constexpr std::string_view kInconsistentTimeline =
    "video.inconsistent_timeline";
/// Complete average and nominal frame rates are unavailable.
inline constexpr std::string_view kMissingFrameRate =
    "video.missing_frame_rate";
/// Average and nominal frame rates disagree.
inline constexpr std::string_view kVariableFrameRate =
    "video.variable_frame_rate";
/// The constant video frame rate differs from the timeline frame rate.
inline constexpr std::string_view kFrameRateMismatch =
    "video.frame_rate_mismatch";
/// No embedded starting timecode was found.
inline constexpr std::string_view kMissingTimecode = "video.missing_timecode";
/// Embedded timecode is malformed, ambiguous, or invalid for the frame rate.
inline constexpr std::string_view kInvalidTimecode = "video.invalid_timecode";
/// Embedded timecode uses a different drop-frame mode from the timeline.
inline constexpr std::string_view kTimecodeModeMismatch =
    "video.timecode_mode_mismatch";
/// Embedded timecode does not identify the first required record frame.
inline constexpr std::string_view kTimelineStartMismatch =
    "video.timeline_start_mismatch";
/// No reliable video duration could be resolved.
inline constexpr std::string_view kMissingDuration = "video.missing_duration";
/// Video and record-timeline durations differ beyond the accepted tolerance.
inline constexpr std::string_view kDurationMismatch = "video.duration_mismatch";

} // namespace timeline_video_diagnostic_code

/// Exact mapping between an accepted video and its record timeline.
struct TimelineVideoMapping final {
    /// Embedded starting timecode read from the video.
    core::Timecode video_start_timecode;
    /// Signed value added to a record frame count to obtain a video frame.
    std::int64_t record_to_video_frame_offset;
    /// First record-timeline frame required by the imported events.
    std::int64_t record_start_frame;
    /// Exclusive last record-timeline frame required by the imported events.
    std::int64_t record_end_frame_exclusive;
    /// Duration reported by the selected video stream, in frames.
    std::int64_t video_duration_frames;

    /// Compares all mapping values.
    bool operator==(const TimelineVideoMapping &) const = default;
};

/// A rejected media/timeline combination and its structured diagnostics.
struct TimelineVideoValidationFailure final {
    /// Every condition that prevents the video from being used.
    std::vector<core::Diagnostic> diagnostics;
};

/// Result of validating already-inspected media information.
using TimelineVideoValidationResult =
    std::expected<TimelineVideoMapping, TimelineVideoValidationFailure>;

/// A validated video whose decoder remains available for frame extraction.
struct TimelineVideoInspectionReceipt final {
    /// The opened decoder positioned at its initial state.
    std::unique_ptr<media::VideoDecoder> decoder;
    /// The validated record-timeline mapping.
    TimelineVideoMapping mapping;
};

/// A rejected video path and its structured failure details.
struct TimelineVideoInspectionFailure final {
    /// The candidate video path.
    std::filesystem::path path;
    /// The decoder failure when opening or inspecting the media failed.
    std::optional<media::VideoDecoderFailure> decoder_failure;
    /// Every condition that prevents the video from being used.
    std::vector<core::Diagnostic> diagnostics;
};

/// Result of opening and validating a rendered timeline video.
using TimelineVideoInspectionResult =
    std::expected<TimelineVideoInspectionReceipt,
                  TimelineVideoInspectionFailure>;

/// Validates rendered video inputs against imported record timelines.
class TimelineVideoInspectionService final {
  public:
    /// Constructs a stateless inspection service.
    TimelineVideoInspectionService(void) = default;

    /// Validates metadata that has already been read from a video.
    [[nodiscard]] TimelineVideoValidationResult
    Validate(const media::VideoMediaInfo &media_information,
             const core::TimelineDocument &timeline) const;

    /// Opens and validates a candidate rendered video.
    ///
    /// A successful result retains the decoder so later services can extract
    /// frames without reopening the file.
    [[nodiscard]] TimelineVideoInspectionResult
    Inspect(const std::filesystem::path &path,
            const core::TimelineDocument &timeline) const;
};

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_TIMELINE_VIDEO_INSPECTION_SERVICE_HPP_
