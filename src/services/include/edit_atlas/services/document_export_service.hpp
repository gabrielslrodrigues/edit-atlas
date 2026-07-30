#ifndef EDIT_ATLAS_SERVICES_DOCUMENT_EXPORT_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_DOCUMENT_EXPORT_SERVICE_HPP_

#include <edit_atlas/core/document_pipeline.hpp>
#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace edit_atlas::services {

/// Describes one request to export a document to a local file.
struct ExportDocumentRequest final {
    std::filesystem::path path;
    std::string format_identifier;
    core::TimelineDocument document;
    std::vector<core::MetadataEntry> options;
    bool replace_existing;
};

/// Identifies the stage at which exporting a document failed.
enum class DocumentExportFailureKind {
    kExportFailed,
    kDestinationExists,
    kWriteFailed,
    kCommitFailed,
};

/// Contains the destination and diagnostics from a successful export.
struct DocumentExportReceipt final {
    std::filesystem::path path;
    std::vector<core::Diagnostic> diagnostics;
};

/// Contains a presentation-neutral exporter or filesystem failure.
struct DocumentExportFailure final {
    std::filesystem::path path;
    DocumentExportFailureKind kind;
    std::error_code filesystem_error;
    std::vector<core::Diagnostic> diagnostics;
};

using ExportDocumentResult =
    std::expected<DocumentExportReceipt, DocumentExportFailure>;

/// Coordinates format-independent export with atomic local-file output.
///
/// The registry must outlive the service. Scheduling, overwrite confirmation,
/// and user feedback are responsibilities of the calling frontend.
class DocumentExportService final {
  public:
    explicit DocumentExportService(
        const core::FormatRegistry &registry) noexcept;

    [[nodiscard]] ExportDocumentResult
    ExportDocument(ExportDocumentRequest request) const;

  private:
    core::DocumentPipeline pipeline_;
};

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_DOCUMENT_EXPORT_SERVICE_HPP_
