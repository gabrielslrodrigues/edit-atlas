#ifndef EDIT_ATLAS_CORE_DOCUMENT_PIPELINE_HPP_
#define EDIT_ATLAS_CORE_DOCUMENT_PIPELINE_HPP_

#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <optional>
#include <string_view>

namespace edit_atlas::core {

/// Stable diagnostic codes emitted by `DocumentPipeline`.
namespace pipeline_diagnostic_code {

/// No importer is registered for an explicitly requested format.
inline constexpr std::string_view kUnknownImportFormat =
    "pipeline.import.unknown_format";
/// Multiple importers produced an equally strong match.
inline constexpr std::string_view kAmbiguousImportFormat =
    "pipeline.import.ambiguous_format";
/// An importer threw while probing content.
inline constexpr std::string_view kProbeException =
    "pipeline.import.probe_exception";
/// An importer threw while converting content.
inline constexpr std::string_view kImportException =
    "pipeline.import.exception";
/// An importer reported success without producing a document.
inline constexpr std::string_view kImportProducedNoDocument =
    "pipeline.import.no_document";
/// No exporter is registered for the requested format.
inline constexpr std::string_view kUnknownExportFormat =
    "pipeline.export.unknown_format";
/// An exporter threw while producing an artifact.
inline constexpr std::string_view kExportException =
    "pipeline.export.exception";
/// An exporter reported success without producing an artifact.
inline constexpr std::string_view kExportProducedNoArtifact =
    "pipeline.export.no_artifact";

} // namespace pipeline_diagnostic_code

/// Selects registered handlers and contains failures at the application edge.
///
/// The registry must outlive the pipeline. Handler exceptions are converted to
/// structured diagnostics instead of escaping through the pipeline interface.
class DocumentPipeline final {
  public:
    /// Creates a pipeline that borrows \p registry.
    ///
    /// \param registry Registry that must outlive the pipeline.
    explicit DocumentPipeline(const FormatRegistry &registry) noexcept;

    /// Imports content using an explicit format or automatic discovery.
    ///
    /// Without \p format_identifier, content probes take precedence and a
    /// unique matching extension breaks equal-confidence ties.
    ///
    /// \param request Source bytes and format-specific options.
    /// \param format_identifier Explicit format key, or no value to probe.
    /// \returns The imported document and structured diagnostics.
    [[nodiscard]] ImportResult Import(
        const ImportRequest &request,
        std::optional<std::string_view> format_identifier = std::nullopt) const;

    /// Exports a document using a stable format identifier.
    ///
    /// \param request Document and format-specific options.
    /// \param format_identifier Stable key of a registered exporter.
    /// \returns The generated artifact and structured diagnostics.
    [[nodiscard]] ExportResult Export(const ExportRequest &request,
                                      std::string_view format_identifier) const;

  private:
    const FormatRegistry &registry_;
};

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_DOCUMENT_PIPELINE_HPP_
