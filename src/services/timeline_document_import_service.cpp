#include <edit_atlas/services/timeline_document_import_service.hpp>

#include <edit_atlas/core/format.hpp>

#include <edit_atlas/storage/local_file.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace edit_atlas::services {
namespace {

[[nodiscard]] std::string Utf8Path(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return std::string{reinterpret_cast<const char *>(utf8.data()),
                       utf8.size()};
}

[[nodiscard]] TimelineDocumentImportFailure
FileFailure(const storage::LocalFileFailure &failure) {
    const auto kind = failure.kind == storage::LocalFileFailureKind::kOpenFailed
                          ? TimelineDocumentImportFailureKind::kOpenFailed
                          : TimelineDocumentImportFailureKind::kReadFailed;
    return TimelineDocumentImportFailure{
        .path = failure.path,
        .kind = kind,
        .filesystem_error = failure.filesystem_error,
        .diagnostics = {},
    };
}

} // namespace

TimelineDocumentImportService::TimelineDocumentImportService(
    const core::FormatRegistry &registry) noexcept
    : pipeline_(registry) {}

TimelineDocumentImportResult TimelineDocumentImportService::Import(
    TimelineDocumentImportRequest request) const {
    auto content = storage::ReadLocalFile(request.path);
    if (!content.has_value()) {
        return std::unexpected(FileFailure(content.error()));
    }

    auto extension = Utf8Path(request.path.extension());
    if (extension.starts_with('.')) {
        extension.erase(extension.begin());
    }
    const core::ImportRequest import_request{
        .content = *content,
        .source_name = Utf8Path(request.path),
        .extension = std::move(extension),
        .options = std::move(request.options),
    };
    const auto format_identifier =
        request.format_identifier.has_value()
            ? std::optional<std::string_view>{*request.format_identifier}
            : std::nullopt;
    auto import_result = pipeline_.Import(import_request, format_identifier);
    if (!import_result.document.has_value()) {
        return std::unexpected(TimelineDocumentImportFailure{
            .path = std::move(request.path),
            .kind = TimelineDocumentImportFailureKind::kImportFailed,
            .filesystem_error = {},
            .diagnostics = std::move(import_result.diagnostics),
        });
    }

    return TimelineDocumentImportReceipt{
        .path = std::move(request.path),
        .timeline = std::move(*import_result.document),
        .diagnostics = std::move(import_result.diagnostics),
    };
}

} // namespace edit_atlas::services
