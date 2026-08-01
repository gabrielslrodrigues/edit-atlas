#ifndef EDIT_ATLAS_SERVICES_TIMELINE_FILTER_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_FILTER_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace edit_atlas::services {

/// Identifies a field that supports free-text matching.
enum class TimelineTextFilterField {
    /// Searches the source event identifier.
    kEventIdentifier,
    /// Searches the source reel identifier.
    kReel,
    /// Searches the source track identifier.
    kTrackIdentifier,
    /// Searches the imported clip name.
    kClip,
    /// Searches human-authored event comments.
    kComments,
};

/// Identifies a field that supports exact timecode matching.
enum class TimelineTimecodeFilterField {
    /// Compares the inclusive source timecode.
    kSourceIn,
    /// Compares the exclusive source timecode.
    kSourceOut,
    /// Compares the inclusive record timecode.
    kRecordIn,
    /// Compares the exclusive record timecode.
    kRecordOut,
};

/// Selects how multiple non-empty filter conditions are combined.
enum class TimelineFilterCombination {
    /// Every condition must match an event.
    kAll,
    /// At least one condition must match an event.
    kAny,
};

/// A text-search condition over a free-text event field.
struct TimelineTextFilterCondition final {
    /// The free-text event field searched by this condition.
    TimelineTextFilterField field = TimelineTextFilterField::kEventIdentifier;
    /// UTF-8 text to find. Empty conditions are ignored.
    std::string text;
    /// Requires matching character case when true.
    bool match_case = false;
    /// Requires the match to be bounded by complete Unicode words when true.
    bool match_whole_word = false;
    /// Interprets the text as an RE2 regular expression when true.
    bool regular_expression = false;

    /// Compares the field, search text, and matching options.
    bool operator==(const TimelineTextFilterCondition &) const = default;
};

/// Selects events with one exact track kind.
struct TimelineTrackKindFilterCondition final {
    /// Required domain track kind.
    core::TrackKind track_kind = core::TrackKind::kVideo;

    bool operator==(const TimelineTrackKindFilterCondition &) const = default;
};

/// Selects events with one exact edit type.
struct TimelineEditTypeFilterCondition final {
    /// Required domain edit type.
    core::EditType edit_type = core::EditType::kCut;

    bool operator==(const TimelineEditTypeFilterCondition &) const = default;
};

/// Selects events with an exact timecode in one timeline range field.
struct TimelineTimecodeFilterCondition final {
    /// Timeline endpoint compared by the condition.
    TimelineTimecodeFilterField field = TimelineTimecodeFilterField::kSourceIn;
    /// Canonical `HH:MM:SS:FF` or drop-frame `HH:MM:SS;FF` text.
    std::string timecode;

    bool operator==(const TimelineTimecodeFilterCondition &) const = default;
};

/// Selects events with an exact record duration in frames.
struct TimelineDurationFilterCondition final {
    /// Empty conditions are ignored.
    std::optional<std::int64_t> frames = std::nullopt;

    bool operator==(const TimelineDurationFilterCondition &) const = default;
};

/// One strongly typed timeline filter condition.
using TimelineFilterCondition =
    std::variant<TimelineTextFilterCondition, TimelineTrackKindFilterCondition,
                 TimelineEditTypeFilterCondition,
                 TimelineTimecodeFilterCondition,
                 TimelineDurationFilterCondition>;

/// A reusable, presentation-independent timeline event query.
struct TimelineFilterQuery final {
    /// How the query combines its non-empty conditions.
    TimelineFilterCombination combination = TimelineFilterCombination::kAll;
    /// Conditions evaluated in their original order.
    std::vector<TimelineFilterCondition> conditions;

    /// Compares the combination and every condition.
    bool operator==(const TimelineFilterQuery &) const = default;
};

/// Ordered indices of events selected from an unchanged source document.
using TimelineEventSelection = std::vector<std::size_t>;

/// Describes a filter condition that could not be prepared.
struct TimelineFilterError final {
    /// Zero-based position of the invalid condition in the query.
    std::size_t condition_index;
    /// Human-readable detail reported by the regular-expression engine.
    std::string message;

    /// Compares the condition position and error detail.
    bool operator==(const TimelineFilterError &) const = default;
};

/// Either an ordered event selection or a filter validation error.
using TimelineFilterResult =
    std::expected<TimelineEventSelection, TimelineFilterError>;

/// Returns the source indices of events matching \p query.
///
/// Literal matching is case-insensitive by default. Regular expressions use
/// RE2 syntax and are compiled once per non-empty condition. A query without
/// non-empty conditions selects all events.
[[nodiscard]] TimelineFilterResult
FilterTimelineEvents(const core::TimelineDocument &document,
                     const TimelineFilterQuery &query);

/// Copies \p document while retaining only events referenced by \p selection.
///
/// Invalid indices are ignored. Document metadata, provenance, and diagnostics
/// are preserved.
[[nodiscard]] core::TimelineDocument
SelectTimelineEvents(const core::TimelineDocument &document,
                     std::span<const std::size_t> selection);

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_TIMELINE_FILTER_HPP_
