#include <edit_atlas/core/timecode.hpp>

#include <array>
#include <charconv>
#include <limits>
#include <numeric>
#include <utility>

namespace edit_atlas::core {
namespace {

constexpr std::int64_t kMinutesPerHour = 60;
constexpr std::int64_t kSecondsPerMinute = 60;
constexpr std::int64_t kHoursPerDay = 24;

[[nodiscard]] std::int64_t
NominalFramesPerSecond(const FrameRate &rate) noexcept {
    const auto numerator = static_cast<std::int64_t>(rate.numerator());
    const auto denominator = static_cast<std::int64_t>(rate.denominator());
    const auto quotient = numerator / denominator;
    const auto remainder = numerator % denominator;
    return quotient + (remainder >= ((denominator + 1) / 2) ? 1 : 0);
}

[[nodiscard]] bool IsSupportedDropFrameRate(const FrameRate &rate) noexcept {
    return (rate.numerator() == 30'000 && rate.denominator() == 1'001) ||
           (rate.numerator() == 60'000 && rate.denominator() == 1'001);
}

[[nodiscard]] std::int64_t
DroppedFramesPerMinute(const FrameRate &rate) noexcept {
    return NominalFramesPerSecond(rate) / 15;
}

[[nodiscard]] std::int64_t FramesPerDay(const FrameRate &rate,
                                        TimecodeMode mode) noexcept {
    const auto nominal_rate = NominalFramesPerSecond(rate);
    const auto nominal_frames =
        nominal_rate * kHoursPerDay * kMinutesPerHour * kSecondsPerMinute;

    if (mode == TimecodeMode::kNonDropFrame) {
        return nominal_frames;
    }

    const auto total_minutes = kHoursPerDay * kMinutesPerHour;
    const auto dropped_minutes = total_minutes - (total_minutes / 10);
    return nominal_frames - (DroppedFramesPerMinute(rate) * dropped_minutes);
}

void AppendTwoDigits(std::string &output, std::uint64_t value) {
    std::array<char, 32> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (result.ptr - buffer.data() < 2) {
        output.push_back('0');
    }
    output.append(buffer.data(), result.ptr);
}

} // namespace

std::expected<FrameRate, FrameRateError>
FrameRate::Create(std::int64_t numerator, std::int64_t denominator) noexcept {
    if (numerator <= 0) {
        return std::unexpected(FrameRateError::kNonPositiveNumerator);
    }
    if (denominator <= 0) {
        return std::unexpected(FrameRateError::kNonPositiveDenominator);
    }
    if (numerator > std::numeric_limits<std::int32_t>::max() ||
        denominator > std::numeric_limits<std::int32_t>::max()) {
        return std::unexpected(FrameRateError::kValueOutOfRange);
    }

    const auto divisor = std::gcd(numerator, denominator);
    return FrameRate{static_cast<std::int32_t>(numerator / divisor),
                     static_cast<std::int32_t>(denominator / divisor)};
}

FrameRate::FrameRate(std::int32_t numerator, std::int32_t denominator) noexcept
    : numerator_(numerator), denominator_(denominator) {}

std::int32_t FrameRate::numerator(void) const noexcept {
    return numerator_;
}

std::int32_t FrameRate::denominator(void) const noexcept {
    return denominator_;
}

std::expected<Timecode, TimecodeError>
Timecode::Create(std::int64_t hours, std::int64_t minutes, std::int64_t seconds,
                 std::int64_t frames, FrameRate rate,
                 TimecodeMode mode) noexcept {
    const auto nominal_rate = NominalFramesPerSecond(rate);
    if (nominal_rate <= 0 ||
        nominal_rate > std::numeric_limits<std::int32_t>::max()) {
        return std::unexpected(TimecodeError::kUnsupportedFrameRate);
    }
    if (mode == TimecodeMode::kDropFrame && !IsSupportedDropFrameRate(rate)) {
        return std::unexpected(TimecodeError::kUnsupportedDropFrameRate);
    }
    if (hours < 0 || hours >= kHoursPerDay) {
        return std::unexpected(TimecodeError::kHourOutOfRange);
    }
    if (minutes < 0 || minutes >= kMinutesPerHour) {
        return std::unexpected(TimecodeError::kMinuteOutOfRange);
    }
    if (seconds < 0 || seconds >= kSecondsPerMinute) {
        return std::unexpected(TimecodeError::kSecondOutOfRange);
    }
    if (frames < 0 || frames >= nominal_rate) {
        return std::unexpected(TimecodeError::kFrameOutOfRange);
    }

    if (mode == TimecodeMode::kDropFrame && minutes % 10 != 0 && seconds == 0 &&
        frames < DroppedFramesPerMinute(rate)) {
        return std::unexpected(TimecodeError::kDroppedFrameLabel);
    }

    return Timecode{
        static_cast<std::int32_t>(hours),
        static_cast<std::int32_t>(minutes),
        static_cast<std::int32_t>(seconds),
        static_cast<std::int32_t>(frames),
        rate,
        mode,
    };
}

std::expected<Timecode, TimecodeError>
Timecode::FromFrameCount(std::int64_t frame_count, FrameRate rate,
                         TimecodeMode mode) noexcept {
    const auto nominal_rate = NominalFramesPerSecond(rate);
    if (nominal_rate <= 0 ||
        nominal_rate > std::numeric_limits<std::int32_t>::max()) {
        return std::unexpected(TimecodeError::kUnsupportedFrameRate);
    }
    if (mode == TimecodeMode::kDropFrame && !IsSupportedDropFrameRate(rate)) {
        return std::unexpected(TimecodeError::kUnsupportedDropFrameRate);
    }
    if (frame_count < 0 || frame_count >= FramesPerDay(rate, mode)) {
        return std::unexpected(TimecodeError::kFrameCountOutOfRange);
    }

    auto labeled_frame = frame_count;
    if (mode == TimecodeMode::kDropFrame) {
        const auto dropped_frames = DroppedFramesPerMinute(rate);
        const auto frames_per_minute =
            (nominal_rate * kSecondsPerMinute) - dropped_frames;
        const auto frames_per_ten_minutes =
            (nominal_rate * kSecondsPerMinute * 10) - (dropped_frames * 9);
        const auto ten_minute_blocks = frame_count / frames_per_ten_minutes;
        const auto remaining_frames = frame_count % frames_per_ten_minutes;

        labeled_frame += dropped_frames * 9 * ten_minute_blocks;
        if (remaining_frames > dropped_frames) {
            labeled_frame +=
                dropped_frames *
                ((remaining_frames - dropped_frames) / frames_per_minute);
        }
    }

    const auto frames_per_hour =
        nominal_rate * kSecondsPerMinute * kMinutesPerHour;
    const auto frames_per_minute = nominal_rate * kSecondsPerMinute;
    const auto hours = labeled_frame / frames_per_hour;
    labeled_frame %= frames_per_hour;
    const auto minutes = labeled_frame / frames_per_minute;
    labeled_frame %= frames_per_minute;
    const auto seconds = labeled_frame / nominal_rate;
    const auto frames = labeled_frame % nominal_rate;

    return Create(hours, minutes, seconds, frames, rate, mode);
}

Timecode::Timecode(std::int32_t hours, std::int32_t minutes,
                   std::int32_t seconds, std::int32_t frames, FrameRate rate,
                   TimecodeMode mode) noexcept
    : hours_(hours), minutes_(minutes), seconds_(seconds), frames_(frames),
      rate_(rate), mode_(mode) {}

std::int32_t Timecode::hours(void) const noexcept {
    return hours_;
}

std::int32_t Timecode::minutes(void) const noexcept {
    return minutes_;
}

std::int32_t Timecode::seconds(void) const noexcept {
    return seconds_;
}

std::int32_t Timecode::frames(void) const noexcept {
    return frames_;
}

const FrameRate &Timecode::rate(void) const noexcept {
    return rate_;
}

TimecodeMode Timecode::mode(void) const noexcept {
    return mode_;
}

std::int64_t Timecode::ToFrameCount(void) const noexcept {
    const auto nominal_rate = NominalFramesPerSecond(rate_);
    const auto total_minutes =
        (static_cast<std::int64_t>(hours_) * kMinutesPerHour) + minutes_;
    const auto total_seconds = (total_minutes * kSecondsPerMinute) + seconds_;
    auto frame_count = (total_seconds * nominal_rate) + frames_;

    if (mode_ == TimecodeMode::kDropFrame) {
        const auto dropped_minutes = total_minutes - (total_minutes / 10);
        frame_count -= DroppedFramesPerMinute(rate_) * dropped_minutes;
    }

    return frame_count;
}

std::expected<TimecodeRange, TimecodeRangeError>
TimecodeRange::Create(Timecode start, Timecode end_exclusive) noexcept {
    if (start.rate() != end_exclusive.rate()) {
        return std::unexpected(TimecodeRangeError::kMismatchedFrameRate);
    }
    if (start.mode() != end_exclusive.mode()) {
        return std::unexpected(TimecodeRangeError::kMismatchedMode);
    }
    if (end_exclusive.ToFrameCount() < start.ToFrameCount()) {
        return std::unexpected(TimecodeRangeError::kEndBeforeStart);
    }

    return TimecodeRange{std::move(start), std::move(end_exclusive)};
}

TimecodeRange::TimecodeRange(Timecode start, Timecode end_exclusive) noexcept
    : start_(std::move(start)), end_exclusive_(std::move(end_exclusive)) {}

const Timecode &TimecodeRange::start(void) const noexcept {
    return start_;
}

const Timecode &TimecodeRange::end_exclusive(void) const noexcept {
    return end_exclusive_;
}

std::int64_t TimecodeRange::DurationInFrames(void) const noexcept {
    return end_exclusive_.ToFrameCount() - start_.ToFrameCount();
}

std::string TimecodeRange::Duration(void) const {
    const auto rate = start().rate();
    const auto mode = start().mode();
    const auto nominal_rate =
        static_cast<std::uint64_t>(NominalFramesPerSecond(rate));
    auto labeled_frame = static_cast<std::uint64_t>(DurationInFrames());

    if (mode == TimecodeMode::kDropFrame) {
        const auto dropped_frames =
            static_cast<std::uint64_t>(DroppedFramesPerMinute(rate));
        const auto frames_per_minute =
            (nominal_rate * static_cast<std::uint64_t>(kSecondsPerMinute)) -
            dropped_frames;
        const auto frames_per_ten_minutes =
            (nominal_rate * static_cast<std::uint64_t>(kSecondsPerMinute) *
             10U) -
            (dropped_frames * 9U);
        const auto ten_minute_blocks = labeled_frame / frames_per_ten_minutes;
        const auto remaining_frames = labeled_frame % frames_per_ten_minutes;

        labeled_frame += dropped_frames * 9U * ten_minute_blocks;
        if (remaining_frames > dropped_frames) {
            labeled_frame +=
                dropped_frames *
                ((remaining_frames - dropped_frames) / frames_per_minute);
        }
    }

    const auto frames_per_hour = nominal_rate *
                                 static_cast<std::uint64_t>(kMinutesPerHour) *
                                 static_cast<std::uint64_t>(kSecondsPerMinute);
    const auto frames_per_minute =
        nominal_rate * static_cast<std::uint64_t>(kSecondsPerMinute);
    const auto hours = labeled_frame / frames_per_hour;
    labeled_frame %= frames_per_hour;
    const auto minutes = labeled_frame / frames_per_minute;
    labeled_frame %= frames_per_minute;
    const auto seconds = labeled_frame / nominal_rate;
    const auto frames = labeled_frame % nominal_rate;

    std::string output;
    output.reserve(11);
    AppendTwoDigits(output, hours);
    output.push_back(':');
    AppendTwoDigits(output, minutes);
    output.push_back(':');
    AppendTwoDigits(output, seconds);
    output.push_back(mode == TimecodeMode::kDropFrame ? ';' : ':');
    AppendTwoDigits(output, frames);
    return output;
}

} // namespace edit_atlas::core
