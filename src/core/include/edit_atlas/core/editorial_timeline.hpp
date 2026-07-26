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
    std::string key;
    MetadataValue value;

    bool operator==(const MetadataEntry &) const = default;
};

/// A location in an imported source.
///
/// Lines and columns are one-based. A value of zero means that the
/// corresponding position is unknown.
struct SourceLocation final {
    std::string source;
    std::uint64_t line;
    std::uint64_t column;

    bool operator==(const SourceLocation &) const = default;
};

/// The original source line and the location from which a value was derived.
struct SourceLineProvenance final {
    SourceLocation location;
    std::string line;

    bool operator==(const SourceLineProvenance &) const = default;
};

/// The impact of a diagnostic produced while importing or validating data.
enum class DiagnosticSeverity {
    kInfo,
    kWarning,
    kError,
};

/// A machine-identifiable diagnostic with a human-readable explanation.
struct Diagnostic final {
    DiagnosticSeverity severity;
    std::string code;
    std::string message;
    std::optional<SourceLocation> location;

    bool operator==(const Diagnostic &) const = default;
};

/// The media category addressed by a track.
enum class TrackKind {
    kVideo,
    kAudio,
    kData,
    kOther,
};

/// A format-independent track category and its source identifier.
struct Track final {
    TrackKind kind;
    std::string identifier;

    bool operator==(const Track &) const = default;
};

/// The editorial operation represented by an event.
enum class EditType {
    kCut,
    kDissolve,
    kWipe,
    kKey,
    kOther,
};

/// Additional data for an edit that spans multiple frames.
struct Transition final {
    /// A source-defined transition name or code.
    std::string identifier;

    /// The transition duration in frames.
    std::uint64_t duration_frames;

    bool operator==(const Transition &) const = default;
};

/// A human-authored annotation associated with an edit event.
struct Comment final {
    std::string text;
    std::optional<SourceLineProvenance> provenance;

    bool operator==(const Comment &) const = default;
};

/// A single editorial event with source and destination ranges.
///
/// Source and record ranges are half-open. Each range retains its own frame
/// rate, allowing future importers to represent mixed-rate source material.
struct EditEvent final {
    std::string identifier;
    std::string reel;
    Track track;
    EditType edit_type;
    std::optional<Transition> transition;
    TimecodeRange source_range;
    TimecodeRange record_range;
    std::vector<Comment> comments;
    std::vector<MetadataEntry> metadata;
    std::optional<SourceLineProvenance> provenance;

    bool operator==(const EditEvent &) const = default;
};

/// A format-independent editorial timeline and its import diagnostics.
///
/// `frame_rate` and `timecode_mode` describe the document's primary time base.
/// Individual edit ranges remain self-describing and may use other rates.
struct TimelineDocument final {
    std::string title;
    FrameRate frame_rate;
    TimecodeMode timecode_mode;
    std::vector<EditEvent> events;
    std::vector<MetadataEntry> metadata;
    std::vector<Diagnostic> diagnostics;
    std::optional<SourceLineProvenance> provenance;

    bool operator==(const TimelineDocument &) const = default;
};

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_EDITORIAL_TIMELINE_HPP_
