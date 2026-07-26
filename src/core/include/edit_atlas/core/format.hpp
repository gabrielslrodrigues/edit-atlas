#ifndef EDIT_ATLAS_CORE_FORMAT_HPP_
#define EDIT_ATLAS_CORE_FORMAT_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

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
    std::string identifier;
    std::string display_name;
    std::vector<std::string> extensions;

    bool operator==(const FormatDescriptor &) const = default;
};

/// Expresses how confidently an importer recognizes content.
enum class ProbeConfidence {
    kNone = 0,
    kLow = 25,
    kMedium = 50,
    kHigh = 75,
    kCertain = 100,
};

/// Input content and options passed to an importer.
///
/// The content remains owned by the caller and is valid only for the duration
/// of an importer call. `extension` may be empty and omits the leading dot.
struct ImportRequest final {
    std::span<const std::byte> content;
    std::string source_name;
    std::string extension;
    std::vector<MetadataEntry> options;
};

/// The result of importing editorial data.
///
/// A failed import has no document and contains at least one error diagnostic.
/// Successful imports may still contain warnings.
struct ImportResult final {
    std::optional<TimelineDocument> document;
    std::vector<Diagnostic> diagnostics;
};

/// A document and format-specific options passed to an exporter.
struct ExportRequest final {
    const TimelineDocument &document;
    std::vector<MetadataEntry> options;
};

/// Bytes produced by an exporter and their suggested file information.
struct ExportArtifact final {
    std::vector<std::byte> content;
    std::string suggested_extension;
    std::string media_type;
};

/// The result of exporting an editorial document.
///
/// A failed export has no artifact and contains at least one error diagnostic.
/// Successful exports may still contain warnings.
struct ExportResult final {
    std::optional<ExportArtifact> artifact;
    std::vector<Diagnostic> diagnostics;
};

/// Converts in-memory content into the format-independent timeline model.
class Importer {
  public:
    virtual ~Importer(void) = default;

    Importer(const Importer &) = delete;
    Importer &operator=(const Importer &) = delete;
    Importer(Importer &&) = delete;
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
    Importer(void) = default;
};

/// Converts a timeline document into an interchange or report format.
class Exporter {
  public:
    virtual ~Exporter(void) = default;

    Exporter(const Exporter &) = delete;
    Exporter &operator=(const Exporter &) = delete;
    Exporter(Exporter &&) = delete;
    Exporter &operator=(Exporter &&) = delete;

    /// Returns the stable format description owned by this exporter.
    [[nodiscard]] virtual const FormatDescriptor &
    descriptor(void) const noexcept = 0;

    /// Exports a document without relying on GUI or filesystem state.
    [[nodiscard]] virtual ExportResult
    Export(const ExportRequest &request) const = 0;

  protected:
    Exporter(void) = default;
};

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_FORMAT_HPP_
