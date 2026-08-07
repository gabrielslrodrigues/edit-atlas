#ifndef EDIT_ATLAS_CORE_TIMECODE_HPP_
#define EDIT_ATLAS_CORE_TIMECODE_HPP_

#include <cstdint>
#include <expected>
#include <string>

namespace edit_atlas::core {

/// Identifies why a rational frame rate could not be constructed.
enum class FrameRateError {
    /// The numerator is zero or negative.
    kNonPositiveNumerator,
    /// The denominator is zero or negative.
    kNonPositiveDenominator,
    /// A normalized component does not fit in the stored representation.
    kValueOutOfRange,
};

/// An exact, positive rational number of frames per second.
///
/// Frame rates are reduced to their canonical form, so equivalent rates such
/// as 48000/2002 and 24000/1001 compare equal. The type deliberately provides
/// no floating-point conversion because editorial frame arithmetic must remain
/// exact.
class FrameRate final {
  public:
    /// Creates and normalizes a frame rate.
    ///
    /// \returns A frame rate, or an error when either component is non-positive
    /// or cannot be represented by the type.
    [[nodiscard]] static std::expected<FrameRate, FrameRateError>
    Create(std::int64_t numerator, std::int64_t denominator) noexcept;

    /// Returns the normalized numerator.
    [[nodiscard]] std::int32_t numerator(void) const noexcept;

    /// Returns the normalized, positive denominator.
    [[nodiscard]] std::int32_t denominator(void) const noexcept;

    /// Compares normalized rational values.
    bool operator==(const FrameRate &) const = default;

  private:
    FrameRate(std::int32_t numerator, std::int32_t denominator) noexcept;

    std::int32_t numerator_;
    std::int32_t denominator_;
};

/// Selects the numbering convention used by a timecode.
enum class TimecodeMode {
    /// Uses every timecode label in sequence.
    kNonDropFrame,

    /// Skips labels to keep NTSC timecode aligned with elapsed clock time.
    kDropFrame,
};

/// Identifies why a timecode could not be constructed or converted.
enum class TimecodeError {
    /// The frame rate has no supported nominal time base.
    kUnsupportedFrameRate,
    /// Drop-frame numbering is not defined for the supplied frame rate.
    kUnsupportedDropFrameRate,
    /// The hour is outside the supported 24-hour day.
    kHourOutOfRange,
    /// The minute is outside `[0, 59]`.
    kMinuteOutOfRange,
    /// The second is outside `[0, 59]`.
    kSecondOutOfRange,
    /// The frame is outside the rate's nominal time base.
    kFrameOutOfRange,
    /// The label is skipped by the drop-frame numbering convention.
    kDroppedFrameLabel,
    /// The frame count is outside the supported 24-hour day.
    kFrameCountOutOfRange,
};

/// A validated SMPTE-style timecode label and its exact frame rate.
///
/// Timecodes cover one 24-hour day, from 00:00:00:00 through the last valid
/// label before 24:00:00:00. Drop-frame labels are supported for 30000/1001
/// and 60000/1001. Instances can only be obtained through validated factories.
class Timecode final {
  public:
    /// Creates a timecode from its displayed components.
    ///
    /// Component ranges and skipped drop-frame labels are validated. Frames
    /// must be below the nominal time base: 24 for 24000/1001, 30 for
    /// 30000/1001, and so on.
    ///
    /// \returns A timecode, or the specific validation error.
    [[nodiscard]] static std::expected<Timecode, TimecodeError>
    Create(std::int64_t hours, std::int64_t minutes, std::int64_t seconds,
           std::int64_t frames, FrameRate rate, TimecodeMode mode) noexcept;

    /// Converts a zero-based frame count within one 24-hour day to timecode.
    ///
    /// Conversion uses integer arithmetic and never wraps at the day boundary.
    ///
    /// \returns A timecode, or an error for an unsupported rate or frame count.
    [[nodiscard]] static std::expected<Timecode, TimecodeError>
    FromFrameCount(std::int64_t frame_count, FrameRate rate,
                   TimecodeMode mode) noexcept;

    /// Returns the hour component in the range [0, 23].
    [[nodiscard]] std::int32_t hours(void) const noexcept;

    /// Returns the minute component in the range [0, 59].
    [[nodiscard]] std::int32_t minutes(void) const noexcept;

    /// Returns the second component in the range [0, 59].
    [[nodiscard]] std::int32_t seconds(void) const noexcept;

    /// Returns the frame component within the nominal time base.
    [[nodiscard]] std::int32_t frames(void) const noexcept;

    /// Returns the exact frame rate associated with this label.
    [[nodiscard]] const FrameRate &rate(void) const noexcept;

    /// Returns the timecode numbering convention.
    [[nodiscard]] TimecodeMode mode(void) const noexcept;

    /// Returns the zero-based frame count represented by this label.
    [[nodiscard]] std::int64_t ToFrameCount(void) const noexcept;

    /// Compares the label, exact frame rate, and numbering mode.
    bool operator==(const Timecode &) const = default;

  private:
    Timecode(std::int32_t hours, std::int32_t minutes, std::int32_t seconds,
             std::int32_t frames, FrameRate rate, TimecodeMode mode) noexcept;

    std::int32_t hours_;
    std::int32_t minutes_;
    std::int32_t seconds_;
    std::int32_t frames_;
    FrameRate rate_;
    TimecodeMode mode_;
};

/// Identifies why a timecode range could not be constructed.
enum class TimecodeRangeError {
    /// The endpoints use different frame rates.
    kMismatchedFrameRate,
    /// The endpoints use different numbering modes.
    kMismatchedMode,
    /// The exclusive end precedes the start.
    kEndBeforeStart,
};

/// A validated half-open timecode range of `[start, end_exclusive)`.
///
/// Both endpoints always have the same frame rate and numbering mode. Empty
/// ranges are valid, while ranges whose end precedes their start are rejected.
class TimecodeRange final {
  public:
    /// Creates a range from compatible endpoints.
    ///
    /// \returns A range, or an error describing the violated invariant.
    [[nodiscard]] static std::expected<TimecodeRange, TimecodeRangeError>
    Create(Timecode start, Timecode end_exclusive) noexcept;

    /// Returns the inclusive start.
    [[nodiscard]] const Timecode &start(void) const noexcept;

    /// Returns the exclusive end.
    [[nodiscard]] const Timecode &end_exclusive(void) const noexcept;

    /// Returns `end_exclusive - start` as an exact frame count.
    [[nodiscard]] std::int64_t DurationInFrames(void) const noexcept;

    /// Formats this elapsed duration as `HH:MM:SS:FF`.
    ///
    /// The hour component does not wrap. Drop-frame ranges use `;` before the
    /// frame component and preserve drop-frame numbering when converting
    /// elapsed frames.
    [[nodiscard]] std::string Duration(void) const;

    /// Compares both validated endpoints.
    bool operator==(const TimecodeRange &) const = default;

  private:
    TimecodeRange(Timecode start, Timecode end_exclusive) noexcept;

    Timecode start_;
    Timecode end_exclusive_;
};

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_TIMECODE_HPP_
