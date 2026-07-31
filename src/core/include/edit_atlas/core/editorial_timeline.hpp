#ifndef EDIT_ATLAS_CORE_EDITORIAL_TIMELINE_HPP_
#define EDIT_ATLAS_CORE_EDITORIAL_TIMELINE_HPP_

#include <edit_atlas/core/timecode.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace edit_atlas::core {

/// A scalar metadata value retained without knowledge of its source format.
using MetadataValue = std::variant<bool, std::int64_t, double, std::string>;

/// A metadata property.
///
/// Metadata is stored as a sequence of entries so importers can preserve source
/// order and duplicate keys when a format permits them.
struct MetadataEntry final {
    /// The source-defined or Edit Atlas option key.
    std::string key;

    /// The value associated with `key`.
    MetadataValue value;

    /// Compares metadata keys and values.
    bool operator==(const MetadataEntry &) const = default;
};

/// A location in an imported source.
///
/// Lines and columns are one-based. A value of zero means that the
/// corresponding position is unknown.
struct SourceLocation final {
    /// The display name or path identifying the source.
    std::string source;

    /// The one-based source line, or zero when unknown.
    std::uint64_t line;

    /// The one-based source column, or zero when unknown.
    std::uint64_t column;

    /// Compares every source-location component.
    bool operator==(const SourceLocation &) const = default;
};

/// The original source line and the location from which a value was derived.
struct SourceLineProvenance final {
    /// The location of the retained line.
    SourceLocation location;

    /// The original source line without its line terminator.
    std::string line;

    /// Compares the source location and retained text.
    bool operator==(const SourceLineProvenance &) const = default;
};

/// The impact of a diagnostic produced while importing or validating data.
enum class DiagnosticSeverity {
    /// Context that does not indicate a problem.
    kInfo,

    /// A recoverable condition that may require attention.
    kWarning,

    /// A condition that prevents the requested operation from succeeding.
    kError,
};

/// A machine-identifiable diagnostic with a human-readable explanation.
struct Diagnostic final {
    /// The impact of the reported condition.
    DiagnosticSeverity severity;

    /// A stable machine-readable diagnostic identifier.
    std::string code;

    /// A human-readable description suitable for presentation.
    std::string message;

    /// The relevant source position, when one is known.
    std::optional<SourceLocation> location;

    /// Compares every diagnostic component.
    bool operator==(const Diagnostic &) const = default;
};

/// The media category addressed by a track.
enum class TrackKind {
    /// Picture media.
    kVideo,

    /// Sound media.
    kAudio,

    /// Non-picture, non-sound data.
    kData,

    /// A source category that cannot be represented more specifically.
    kOther,
};

/// A format-independent track category and its source identifier.
struct Track final {
    /// The normalized media category.
    TrackKind kind;

    /// The original source-format track identifier.
    std::string identifier;

    /// Compares the category and source identifier.
    bool operator==(const Track &) const = default;
};

/// The editorial operation represented by an event.
enum class EditType {
    /// An instantaneous cut between sources.
    kCut,

    /// A dissolve transition.
    kDissolve,

    /// A wipe transition.
    kWipe,

    /// A key or overlay operation.
    kKey,

    /// A source operation that cannot be represented more specifically.
    kOther,
};

/// Additional data for an edit that spans multiple frames.
struct Transition final {
    /// A source-defined transition name or code.
    std::string identifier;

    /// The transition duration in frames.
    std::uint64_t duration_frames;

    /// Compares the identifier and duration.
    bool operator==(const Transition &) const = default;
};

/// A human-authored annotation associated with an edit event.
struct Comment final {
    /// The human-authored comment text.
    std::string text;

    /// The original source line, when retained by the importer.
    std::optional<SourceLineProvenance> provenance;

    /// Compares the text and provenance.
    bool operator==(const Comment &) const = default;
};

/// A single editorial event with source and destination ranges.
///
/// Source and record ranges are half-open. Each range retains its own frame
/// rate, allowing future importers to represent mixed-rate source material.
struct EditEvent final {
    /// The event identifier assigned by the source format.
    std::string identifier;

    /// The source reel or clip identifier.
    std::string reel;

    /// The media track addressed by the event.
    Track track;

    /// The normalized editorial operation.
    EditType edit_type;

    /// Transition details for operations that span multiple frames.
    std::optional<Transition> transition;

    /// The selected range in the source material.
    TimecodeRange source_range;

    /// The destination range in the edited timeline.
    TimecodeRange record_range;

    /// Human-authored annotations in source order.
    std::vector<Comment> comments;

    /// Format-specific event metadata in source order.
    std::vector<MetadataEntry> metadata;

    /// The original event line, when retained by the importer.
    std::optional<SourceLineProvenance> provenance;

    /// Compares every event component.
    bool operator==(const EditEvent &) const = default;
};

/// A format-independent editorial timeline and its import diagnostics.
///
/// `frame_rate` and `timecode_mode` describe the document's primary time base.
/// Individual edit ranges remain self-describing and may use other rates.
struct TimelineDocument final {
    /// The title supplied by the source document.
    std::string title;

    /// The document's primary exact frame rate.
    FrameRate frame_rate;

    /// The document's primary timecode numbering convention.
    TimecodeMode timecode_mode;

    /// Editorial events in timeline order.
    std::vector<EditEvent> events;

    /// Document-level format metadata in source order.
    std::vector<MetadataEntry> metadata;

    /// Diagnostics retained as part of the imported document.
    std::vector<Diagnostic> diagnostics;

    /// The source title line or equivalent document provenance.
    std::optional<SourceLineProvenance> provenance;

    /// Compares every document component.
    bool operator==(const TimelineDocument &) const = default;
};

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_EDITORIAL_TIMELINE_HPP_
