#ifndef EDIT_ATLAS_SERVICES_DOCUMENT_IMPORT_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_DOCUMENT_IMPORT_SERVICE_HPP_

#include <edit_atlas/core/document_pipeline.hpp>
#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace edit_atlas::services {

/// Describes one request to import a local document.
struct ImportDocumentRequest final {
    /// The local source path to read.
    std::filesystem::path path;
    /// An explicit stable importer identifier, or no value to probe content.
    std::optional<std::string> format_identifier;
    /// Format-specific import options.
    std::vector<core::MetadataEntry> options;
};

/// Identifies the stage at which importing a document failed.
enum class DocumentImportFailureKind {
    /// The source file could not be opened.
    kOpenFailed,
    /// The opened source file could not be read completely.
    kReadFailed,
    /// The format-independent pipeline could not import the source.
    kImportFailed,
};

/// Contains a successfully imported document and its source context.
struct DocumentImportReceipt final {
    /// The imported source path.
    std::filesystem::path path;
    /// The complete imported timeline document.
    core::TimelineDocument document;
    /// Non-fatal diagnostics produced during import.
    std::vector<core::Diagnostic> diagnostics;
};

/// Contains a presentation-neutral filesystem or importer failure.
struct DocumentImportFailure final {
    /// The requested source path.
    std::filesystem::path path;
    /// The stage at which import failed.
    DocumentImportFailureKind kind;
    /// The native filesystem error, when failure involved the filesystem.
    std::error_code filesystem_error;
    /// Structured diagnostics produced by the pipeline or importer.
    std::vector<core::Diagnostic> diagnostics;
};

/// Either an imported document receipt or a presentation-neutral failure.
using ImportDocumentResult =
    std::expected<DocumentImportReceipt, DocumentImportFailure>;

/// Coordinates filesystem input with the format-independent import pipeline.
///
/// The registry must outlive the service. Scheduling and user interaction are
/// responsibilities of the calling frontend.
class DocumentImportService final {
  public:
    /// Creates a service backed by a registry that must outlive it.
    explicit DocumentImportService(
        const core::FormatRegistry &registry) noexcept;

    /// Reads and imports the local document described by \p request.
    ///
    /// \returns A receipt on success or a stage-specific failure.
    [[nodiscard]] ImportDocumentResult
    ImportDocument(ImportDocumentRequest request) const;

  private:
    core::DocumentPipeline pipeline_;
};

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_DOCUMENT_IMPORT_SERVICE_HPP_
