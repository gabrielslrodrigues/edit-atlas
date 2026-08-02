#ifndef EDIT_ATLAS_CORE_FORMAT_HPP_
#define EDIT_ATLAS_CORE_FORMAT_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace edit_atlas::core {

/// Describes an editorial interchange format.
///
/// Identifiers are stable, lowercase ASCII keys used in persisted settings and
/// command-line interfaces. Extensions are lowercase and omit the leading dot.
struct FormatDescriptor final {
    /// The stable lowercase ASCII format key.
    std::string identifier;

    /// The human-readable format name.
    std::string display_name;

    /// Supported lowercase extensions without leading dots.
    std::vector<std::string> extensions;

    /// Compares every descriptor component.
    bool operator==(const FormatDescriptor &) const = default;
};

/// Expresses how confidently an importer recognizes content.
enum class ProbeConfidence {
    /// The content does not match the format.
    kNone = 0,
    /// The content weakly resembles the format.
    kLow = 25,
    /// The content contains meaningful format evidence.
    kMedium = 50,
    /// The content strongly matches the format.
    kHigh = 75,
    /// The content contains definitive format evidence.
    kCertain = 100,
};

/// Input content and options passed to an importer.
///
/// The content remains owned by the caller and is valid only for the duration
/// of an importer call. `extension` may be empty and omits the leading dot.
struct ImportRequest final {
    /// Non-owning source bytes valid for the duration of the call.
    std::span<const std::byte> content;

    /// A display name used in diagnostics and provenance.
    std::string source_name;

    /// A lowercase extension without a leading dot, or an empty string.
    std::string extension;

    /// Format-specific options expressed as stable metadata entries.
    std::vector<MetadataEntry> options;
};

/// The result of importing editorial data.
///
/// A failed import has no document and contains at least one error diagnostic.
/// Successful imports may still contain warnings.
struct ImportResult final {
    /// The imported document, absent when import failed.
    std::optional<TimelineDocument> document;

    /// Errors, warnings, and informational diagnostics from the importer.
    std::vector<Diagnostic> diagnostics;
};

/// A document and format-specific options passed to an exporter.
struct ExportRequest final {
    /// The document to export; the caller retains ownership.
    const TimelineDocument &document;

    /// Ordered, unique event fields included in the exported representation.
    /// Defaults to every supported field in the standard report order.
    std::vector<TimelineEventField> event_projection{
        DefaultTimelineEventProjection().begin(),
        DefaultTimelineEventProjection().end(),
    };

    /// Format-specific options expressed as stable metadata entries.
    std::vector<MetadataEntry> options;
};

/// Bytes produced by an exporter and their suggested file information.
struct ExportArtifact final {
    /// The complete generated file contents.
    std::vector<std::byte> content;

    /// The preferred lowercase extension without a leading dot.
    std::string suggested_extension;

    /// The artifact's IANA media type.
    std::string media_type;
};

/// The result of exporting an editorial document.
///
/// A failed export has no artifact and contains at least one error diagnostic.
/// Successful exports may still contain warnings.
struct ExportResult final {
    /// The generated artifact, absent when export failed.
    std::optional<ExportArtifact> artifact;

    /// Errors, warnings, and informational diagnostics from the exporter.
    std::vector<Diagnostic> diagnostics;
};

/// Converts in-memory content into the format-independent timeline model.
class Importer {
  public:
    /// Destroys the importer through its interface.
    virtual ~Importer(void) = default;

    /// Importers are non-copyable.
    Importer(const Importer &) = delete;
    /// Importers are non-copy-assignable.
    Importer &operator=(const Importer &) = delete;
    /// Importers are non-movable.
    Importer(Importer &&) = delete;
    /// Importers are non-move-assignable.
    Importer &operator=(Importer &&) = delete;

    /// Returns the stable format description owned by this importer.
    [[nodiscard]] virtual const FormatDescriptor &
    descriptor(void) const noexcept = 0;

    /// Inspects content without performing a full import.
    ///
    /// Implementations should inspect only enough data to identify their
    /// format and must not retain references into \p request.
    [[nodiscard]] virtual ProbeConfidence
    Probe(const ImportRequest &request) const = 0;

    /// Imports content without relying on GUI or filesystem state.
    [[nodiscard]] virtual ImportResult
    Import(const ImportRequest &request) const = 0;

  protected:
    /// Constructs the importer base for a concrete implementation.
    Importer(void) = default;
};

/// Converts a timeline document into an interchange or report format.
class Exporter {
  public:
    /// Destroys the exporter through its interface.
    virtual ~Exporter(void) = default;

    /// Exporters are non-copyable.
    Exporter(const Exporter &) = delete;
    /// Exporters are non-copy-assignable.
    Exporter &operator=(const Exporter &) = delete;
    /// Exporters are non-movable.
    Exporter(Exporter &&) = delete;
    /// Exporters are non-move-assignable.
    Exporter &operator=(Exporter &&) = delete;

    /// Returns the stable format description owned by this exporter.
    [[nodiscard]] virtual const FormatDescriptor &
    descriptor(void) const noexcept = 0;

    /// Exports a document without relying on GUI or filesystem state.
    [[nodiscard]] virtual ExportResult
    Export(const ExportRequest &request) const = 0;

  protected:
    /// Constructs the exporter base for a concrete implementation.
    Exporter(void) = default;
};

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_FORMAT_HPP_
