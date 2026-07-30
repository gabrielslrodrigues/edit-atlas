#ifndef EDIT_ATLAS_SERVICES_DOCUMENT_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_DOCUMENT_SERVICE_HPP_

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

/// Describes one request to open and import a local document.
struct OpenDocumentRequest final {
    std::filesystem::path path;
    std::optional<std::string> format_identifier;
    std::vector<core::MetadataEntry> options;
};

/// Identifies the stage at which opening a document failed.
enum class DocumentOpenFailureKind {
    kOpenFailed,
    kReadFailed,
    kImportFailed,
};

/// Contains a successfully imported document and its source context.
struct DocumentSession final {
    std::filesystem::path path;
    core::TimelineDocument document;
    std::vector<core::Diagnostic> diagnostics;
};

/// Contains a presentation-neutral filesystem or importer failure.
struct DocumentOpenFailure final {
    std::filesystem::path path;
    DocumentOpenFailureKind kind;
    std::error_code filesystem_error;
    std::vector<core::Diagnostic> diagnostics;
};

using OpenDocumentResult = std::expected<DocumentSession, DocumentOpenFailure>;

/// Coordinates filesystem input with the format-independent import pipeline.
///
/// The registry must outlive the service. Scheduling and user interaction are
/// responsibilities of the calling frontend.
class DocumentService final {
  public:
    explicit DocumentService(const core::FormatRegistry &registry) noexcept;

    [[nodiscard]] OpenDocumentResult
    OpenDocument(OpenDocumentRequest request) const;

  private:
    core::DocumentPipeline pipeline_;
};

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_DOCUMENT_SERVICE_HPP_
