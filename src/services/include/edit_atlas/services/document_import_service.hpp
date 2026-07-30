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
    std::filesystem::path path;
    std::optional<std::string> format_identifier;
    std::vector<core::MetadataEntry> options;
};

/// Identifies the stage at which importing a document failed.
enum class DocumentImportFailureKind {
    kOpenFailed,
    kReadFailed,
    kImportFailed,
};

/// Contains a successfully imported document and its source context.
struct DocumentImportReceipt final {
    std::filesystem::path path;
    core::TimelineDocument document;
    std::vector<core::Diagnostic> diagnostics;
};

/// Contains a presentation-neutral filesystem or importer failure.
struct DocumentImportFailure final {
    std::filesystem::path path;
    DocumentImportFailureKind kind;
    std::error_code filesystem_error;
    std::vector<core::Diagnostic> diagnostics;
};

using ImportDocumentResult =
    std::expected<DocumentImportReceipt, DocumentImportFailure>;

/// Coordinates filesystem input with the format-independent import pipeline.
///
/// The registry must outlive the service. Scheduling and user interaction are
/// responsibilities of the calling frontend.
class DocumentImportService final {
  public:
    explicit DocumentImportService(
        const core::FormatRegistry &registry) noexcept;

    [[nodiscard]] ImportDocumentResult
    ImportDocument(ImportDocumentRequest request) const;

  private:
    core::DocumentPipeline pipeline_;
};

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_DOCUMENT_IMPORT_SERVICE_HPP_
