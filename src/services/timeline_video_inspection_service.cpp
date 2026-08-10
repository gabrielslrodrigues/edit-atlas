#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace edit_atlas::services {
namespace {

struct TimelineSpan final {
    std::int64_t start;
    std::int64_t end_exclusive;
};

[[nodiscard]] core::Diagnostic Error(std::string_view code,
                                     std::string message) {
    return core::Diagnostic{
        .severity = core::DiagnosticSeverity::kError,
        .code = std::string{code},
        .message = std::move(message),
        .location = std::nullopt,
    };
}

[[nodiscard]] bool EqualsAsciiCaseInsensitive(std::string_view left,
                                              std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        auto left_character = left[index];
        auto right_character = right[index];
        if (left_character >= 'A' && left_character <= 'Z') {
            left_character = static_cast<char>(left_character - 'A' + 'a');
        }
        if (right_character >= 'A' && right_character <= 'Z') {
            right_character = static_cast<char>(right_character - 'A' + 'a');
        }
        if (left_character != right_character) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsTimecodeKey(std::string_view key) noexcept {
    constexpr std::string_view kTimecode = "timecode";
    if (EqualsAsciiCaseInsensitive(key, kTimecode)) {
        return true;
    }
    return key.size() > kTimecode.size() &&
           key[key.size() - kTimecode.size() - 1] == '.' &&
           EqualsAsciiCaseInsensitive(key.substr(key.size() - kTimecode.size()),
                                      kTimecode);
}

[[nodiscard]] bool
IsSupportedContainer(media::MediaContainer container) noexcept {
    switch (container) {
    case media::MediaContainer::kMov:
    case media::MediaContainer::kMp4:
    case media::MediaContainer::kMxf:
        return true;
    case media::MediaContainer::kOther:
        return false;
    }
    return false;
}

void AppendTimecodes(const std::vector<media::MediaMetadataEntry> &metadata,
                     std::vector<std::string_view> &values) {
    for (const auto &entry : metadata) {
        if (IsTimecodeKey(entry.key) &&
            std::ranges::find(values, entry.value) == values.end()) {
            values.emplace_back(entry.value);
        }
    }
}

[[nodiscard]] std::vector<std::string_view>
EmbeddedTimecodeValues(const media::VideoMediaInfo &information) {
    std::vector<std::string_view> values;
    const auto &selected_stream =
        information.streams[information.selected_video_stream];
    AppendTimecodes(selected_stream.metadata, values);
    AppendTimecodes(information.metadata, values);
    for (const auto &stream : information.streams) {
        if (stream.index != selected_stream.index) {
            AppendTimecodes(stream.metadata, values);
        }
    }
    return values;
}

[[nodiscard]] std::optional<std::int64_t>
ParseComponent(std::string_view value) noexcept {
    std::int64_t component = 0;
    const auto result =
        std::from_chars(value.data(), value.data() + value.size(), component);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return component;
}

[[nodiscard]] std::optional<core::Timecode>
ParseTimecode(std::string_view value, const core::FrameRate &rate) noexcept {
    if (value.size() != 11 || value[2] != ':' || value[5] != ':' ||
        (value[8] != ':' && value[8] != ';')) {
        return std::nullopt;
    }
    const auto hours = ParseComponent(value.substr(0, 2));
    const auto minutes = ParseComponent(value.substr(3, 2));
    const auto seconds = ParseComponent(value.substr(6, 2));
    const auto frames = ParseComponent(value.substr(9, 2));
    if (!hours.has_value() || !minutes.has_value() || !seconds.has_value() ||
        !frames.has_value()) {
        return std::nullopt;
    }
    const auto mode = value[8] == ';' ? core::TimecodeMode::kDropFrame
                                      : core::TimecodeMode::kNonDropFrame;
    auto timecode =
        core::Timecode::Create(*hours, *minutes, *seconds, *frames, rate, mode);
    if (!timecode.has_value()) {
        return std::nullopt;
    }
    return std::move(*timecode);
}

[[nodiscard]] std::optional<core::FrameRate>
MediaFrameRate(const std::optional<media::Rational> &rate) noexcept {
    if (!rate.has_value()) {
        return std::nullopt;
    }
    auto frame_rate =
        core::FrameRate::Create(rate->numerator, rate->denominator);
    if (!frame_rate.has_value()) {
        return std::nullopt;
    }
    return std::move(*frame_rate);
}

[[nodiscard]] std::optional<TimelineSpan>
RecordTimelineSpan(const core::TimelineDocument &timeline) noexcept {
    if (timeline.events.empty()) {
        return std::nullopt;
    }
    auto start = std::numeric_limits<std::int64_t>::max();
    auto end = std::numeric_limits<std::int64_t>::min();
    for (const auto &event : timeline.events) {
        const auto &range = event.record_range;
        if (range.start().rate() != timeline.frame_rate ||
            range.start().mode() != timeline.timecode_mode) {
            return std::nullopt;
        }
        start = std::min(start, range.start().ToFrameCount());
        end = std::max(end, range.end_exclusive().ToFrameCount());
    }
    return TimelineSpan{.start = start, .end_exclusive = end};
}

[[nodiscard]] std::optional<std::int64_t>
DurationFrames(const media::VideoMediaInfo &information,
               const core::FrameRate &frame_rate) noexcept {
    const auto &stream = information.streams[information.selected_video_stream];
    if (stream.frame_count.has_value() && *stream.frame_count > 0) {
        return stream.frame_count;
    }

    long double seconds = 0.0L;
    if (stream.duration.has_value() && *stream.duration > 0 &&
        stream.time_base.has_value()) {
        seconds = static_cast<long double>(*stream.duration) *
                  static_cast<long double>(stream.time_base->numerator) /
                  static_cast<long double>(stream.time_base->denominator);
    } else if (information.duration_microseconds.has_value()) {
        constexpr long double kMicrosecondsPerSecond = 1'000'000.0L;
        seconds = static_cast<long double>(*information.duration_microseconds) /
                  kMicrosecondsPerSecond;
    } else {
        return std::nullopt;
    }

    const auto frames = seconds *
                        static_cast<long double>(frame_rate.numerator()) /
                        static_cast<long double>(frame_rate.denominator());
    if (!std::isfinite(frames) || frames <= 0.0L ||
        frames > static_cast<long double>(
                     std::numeric_limits<std::int64_t>::max())) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(std::llround(frames));
}

[[nodiscard]] TimelineVideoValidationFailure
Failure(std::vector<core::Diagnostic> diagnostics) {
    return TimelineVideoValidationFailure{
        .diagnostics = std::move(diagnostics),
    };
}

} // namespace

TimelineVideoValidationResult TimelineVideoInspectionService::Validate(
    const media::VideoMediaInfo &information,
    const core::TimelineDocument &timeline) const {
    std::vector<core::Diagnostic> diagnostics;
    if (information.selected_video_stream >= information.streams.size()) {
        diagnostics.push_back(Error(
            timeline_video_diagnostic_code::kOpenFailed,
            "The inspected media does not identify a usable video stream."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }
    if (!IsSupportedContainer(information.container)) {
        diagnostics.push_back(
            Error(timeline_video_diagnostic_code::kUnsupportedContainer,
                  "The inspected media is not a supported MOV, MP4, or MXF "
                  "container."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }

    const auto timeline_span = RecordTimelineSpan(timeline);
    if (!timeline_span.has_value()) {
        const auto code =
            timeline.events.empty()
                ? timeline_video_diagnostic_code::kEmptyTimeline
                : timeline_video_diagnostic_code::kInconsistentTimeline;
        const auto message = timeline.events.empty()
                                 ? "The imported timeline contains no events."
                                 : "The timeline record ranges do not use its "
                                   "primary frame rate and timecode mode.";
        diagnostics.push_back(Error(code, message));
        return std::unexpected(Failure(std::move(diagnostics)));
    }

    const auto &stream = information.streams[information.selected_video_stream];
    const auto average_rate = MediaFrameRate(stream.average_frame_rate);
    const auto nominal_rate = MediaFrameRate(stream.nominal_frame_rate);
    if (!average_rate.has_value() || !nominal_rate.has_value()) {
        diagnostics.push_back(Error(
            timeline_video_diagnostic_code::kMissingFrameRate,
            "The video does not report complete average and nominal frame "
            "rates, so constant-frame-rate timing cannot be verified."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }
    if (*average_rate != *nominal_rate) {
        diagnostics.push_back(
            Error(timeline_video_diagnostic_code::kVariableFrameRate,
                  "The video's average and nominal frame rates differ; a "
                  "constant-frame-rate render is required."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }
    if (*average_rate != timeline.frame_rate) {
        diagnostics.push_back(Error(
            timeline_video_diagnostic_code::kFrameRateMismatch,
            "The video frame rate does not match the imported timeline frame "
            "rate."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }

    const auto timecode_values = EmbeddedTimecodeValues(information);
    if (timecode_values.empty()) {
        diagnostics.push_back(Error(
            timeline_video_diagnostic_code::kMissingTimecode,
            "The video contains no readable embedded starting timecode."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }
    if (timecode_values.size() != 1) {
        diagnostics.push_back(
            Error(timeline_video_diagnostic_code::kInvalidTimecode,
                  "The video reports conflicting embedded timecode values."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }
    auto start_timecode = ParseTimecode(timecode_values.front(), *average_rate);
    if (!start_timecode.has_value()) {
        diagnostics.push_back(Error(
            timeline_video_diagnostic_code::kInvalidTimecode,
            "The video's embedded starting timecode is unreadable or invalid "
            "for its frame rate."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }
    if (start_timecode->mode() != timeline.timecode_mode) {
        diagnostics.push_back(Error(
            timeline_video_diagnostic_code::kTimecodeModeMismatch,
            "The video's embedded timecode mode does not match the imported "
            "timeline."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }
    if (start_timecode->ToFrameCount() != timeline_span->start) {
        diagnostics.push_back(Error(
            timeline_video_diagnostic_code::kTimelineStartMismatch,
            "The video's embedded starting timecode does not match the first "
            "record-timeline frame."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }

    const auto duration = DurationFrames(information, *average_rate);
    if (!duration.has_value()) {
        diagnostics.push_back(
            Error(timeline_video_diagnostic_code::kMissingDuration,
                  "The video does not report a usable duration."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }
    const auto timeline_duration =
        timeline_span->end_exclusive - timeline_span->start;
    const auto duration_difference = *duration - timeline_duration;
    if (duration_difference < -kVideoDurationToleranceFrames ||
        duration_difference > kVideoDurationToleranceFrames) {
        diagnostics.push_back(Error(
            timeline_video_diagnostic_code::kDurationMismatch,
            "The video duration differs from the required record-timeline "
            "span by more than one frame."));
        return std::unexpected(Failure(std::move(diagnostics)));
    }

    return TimelineVideoMapping{
        .video_start_timecode = std::move(*start_timecode),
        .record_to_video_frame_offset = -timeline_span->start,
        .record_start_frame = timeline_span->start,
        .record_end_frame_exclusive = timeline_span->end_exclusive,
        .video_duration_frames = *duration,
    };
}

TimelineVideoInspectionResult TimelineVideoInspectionService::Inspect(
    const std::filesystem::path &path,
    const core::TimelineDocument &timeline) const {
    auto decoder = media::VideoDecoder::Open(path);
    if (!decoder.has_value()) {
        auto decoder_failure = std::move(decoder.error());
        std::vector<core::Diagnostic> diagnostics;
        diagnostics.push_back(Error(timeline_video_diagnostic_code::kOpenFailed,
                                    decoder_failure.message));
        return std::unexpected(TimelineVideoInspectionFailure{
            .path = path,
            .decoder_failure = std::move(decoder_failure),
            .diagnostics = std::move(diagnostics),
        });
    }

    auto validation = Validate((*decoder)->Information(), timeline);
    if (!validation.has_value()) {
        return std::unexpected(TimelineVideoInspectionFailure{
            .path = path,
            .decoder_failure = std::nullopt,
            .diagnostics = std::move(validation.error().diagnostics),
        });
    }
    return TimelineVideoInspectionReceipt{
        .decoder = std::move(*decoder),
        .mapping = std::move(*validation),
    };
}

} // namespace edit_atlas::services
