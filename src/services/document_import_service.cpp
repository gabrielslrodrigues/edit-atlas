#include <edit_atlas/services/document_import_service.hpp>

#include <edit_atlas/core/format.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace edit_atlas::services {
namespace {

[[nodiscard]] std::string Utf8Path(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return std::string{reinterpret_cast<const char *>(utf8.data()),
                       utf8.size()};
}

[[nodiscard]] std::unexpected<DocumentImportFailure>
FileFailure(const ImportDocumentRequest &request,
            DocumentImportFailureKind kind, std::error_code error) {
    if (!error) {
        error = std::make_error_code(std::errc::io_error);
    }
    return std::unexpected(DocumentImportFailure{
        .path = request.path,
        .kind = kind,
        .filesystem_error = error,
        .diagnostics = {},
    });
}

} // namespace

DocumentImportService::DocumentImportService(
    const core::FormatRegistry &registry) noexcept
    : pipeline_(registry) {}

ImportDocumentResult
DocumentImportService::ImportDocument(ImportDocumentRequest request) const {
    errno = 0;
    std::ifstream input{request.path, std::ios::binary};
    if (!input.is_open()) {
        return FileFailure(request, DocumentImportFailureKind::kOpenFailed,
                           std::error_code{errno, std::generic_category()});
    }

    std::error_code size_error;
    const auto file_size = std::filesystem::file_size(request.path, size_error);
    if (size_error) {
        return FileFailure(request, DocumentImportFailureKind::kReadFailed,
                           size_error);
    }
    if (file_size > std::numeric_limits<std::size_t>::max() ||
        file_size > static_cast<std::uintmax_t>(
                        std::numeric_limits<std::streamsize>::max())) {
        return FileFailure(request, DocumentImportFailureKind::kReadFailed,
                           std::make_error_code(std::errc::value_too_large));
    }

    std::vector<std::byte> content(static_cast<std::size_t>(file_size));
    if (!content.empty()) {
        errno = 0;
        input.read(reinterpret_cast<char *>(content.data()),
                   static_cast<std::streamsize>(content.size()));
        if (!input) {
            return FileFailure(request, DocumentImportFailureKind::kReadFailed,
                               std::error_code{errno, std::generic_category()});
        }
    }

    auto extension = Utf8Path(request.path.extension());
    if (extension.starts_with('.')) {
        extension.erase(extension.begin());
    }
    const core::ImportRequest import_request{
        .content = content,
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
        return std::unexpected(DocumentImportFailure{
            .path = std::move(request.path),
            .kind = DocumentImportFailureKind::kImportFailed,
            .filesystem_error = {},
            .diagnostics = std::move(import_result.diagnostics),
        });
    }

    return DocumentImportReceipt{
        .path = std::move(request.path),
        .document = std::move(*import_result.document),
        .diagnostics = std::move(import_result.diagnostics),
    };
}

} // namespace edit_atlas::services
