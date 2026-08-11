#ifndef EDIT_ATLAS_CORE_TIMELINE_PROJECTION_HPP_
#define EDIT_ATLAS_CORE_TIMELINE_PROJECTION_HPP_

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace edit_atlas::core {

/// Identifies one exportable field of an editorial timeline event.
enum class TimelineEventField {
    /// Source event identifier.
    kEventIdentifier,
    /// Initial rendered-video frame associated with the event.
    kInitialFrame,
    /// Source reel identifier.
    kReel,
    /// Domain track kind.
    kTrackKind,
    /// Source track identifier.
    kTrackIdentifier,
    /// Editorial operation type.
    kEditType,
    /// Transition identifier.
    kTransitionIdentifier,
    /// Transition duration in frames.
    kTransitionDuration,
    /// Inclusive source timecode.
    kSourceIn,
    /// Exclusive source timecode.
    kSourceOut,
    /// Inclusive record timecode.
    kRecordIn,
    /// Exclusive record timecode.
    kRecordOut,
    /// Formatted record duration.
    kDuration,
    /// Record duration in frames.
    kDurationFrames,
    /// Imported clip name.
    kClipName,
    /// Imported source filename.
    kSourceFile,
    /// Human-authored comments.
    kComments,
    /// Original source line number.
    kSourceLine,
    /// Number of defined timeline event fields.
    kCount,
};

/// Number of known timeline event fields, including optional fields.
inline constexpr std::size_t kTimelineEventFieldCount =
    static_cast<std::size_t>(TimelineEventField::kCount);

/// Returns the stable lowercase identifier for \p field.
///
/// The returned view has static storage duration. An unknown enum value returns
/// an empty view.
[[nodiscard]] std::string_view
TimelineEventFieldIdentifier(TimelineEventField field) noexcept;

/// Resolves the stable timeline event field \p identifier.
[[nodiscard]] std::optional<TimelineEventField>
TimelineEventFieldFromIdentifier(std::string_view identifier) noexcept;

/// Returns every known timeline event field in standard selection order.
///
/// The returned view has static storage duration.
[[nodiscard]] std::span<const TimelineEventField>
TimelineEventFields(void) noexcept;

/// Returns every timeline event field in the default export order.
///
/// Optional fields that require supplemental data, such as `kInitialFrame`,
/// are not selected by default.
///
/// The returned view has static storage duration.
[[nodiscard]] std::span<const TimelineEventField>
DefaultTimelineEventProjection(void) noexcept;

/// Returns whether \p projection is non-empty and contains unique known fields.
[[nodiscard]] bool IsValidTimelineEventProjection(
    std::span<const TimelineEventField> projection) noexcept;

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_TIMELINE_PROJECTION_HPP_
