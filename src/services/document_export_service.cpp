#include <edit_atlas/services/document_export_service.hpp>

#include <edit_atlas/core/format.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <random>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace edit_atlas::services {
namespace {

constexpr std::size_t kMaximumTemporaryPathAttempts = 16;

class TemporaryFile final {
  public:
    explicit TemporaryFile(std::filesystem::path path)
        : path_(std::move(path)) {}

    ~TemporaryFile(void) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove(path_, error));
    }

    TemporaryFile(const TemporaryFile &) = delete;
    TemporaryFile &operator=(const TemporaryFile &) = delete;
    TemporaryFile(TemporaryFile &&) = delete;
    TemporaryFile &operator=(TemporaryFile &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] std::error_code IoError(void) {
    if (errno != 0) {
        return {errno, std::generic_category()};
    }
    return std::make_error_code(std::errc::io_error);
}

[[nodiscard]] std::filesystem::path
TemporaryPathFor(const std::filesystem::path &destination,
                 std::uint64_t suffix) {
    auto filename = destination.filename();
    filename += ".edit-atlas-";
    filename += std::to_string(suffix);
    filename += ".tmp";
    return destination.parent_path() / filename;
}

[[nodiscard]] std::expected<std::filesystem::path, std::error_code>
AvailableTemporaryPath(const std::filesystem::path &destination) {
    std::random_device random;
    for (std::size_t attempt = 0; attempt < kMaximumTemporaryPathAttempts;
         ++attempt) {
        const auto suffix = (static_cast<std::uint64_t>(random()) << 32U) |
                            static_cast<std::uint64_t>(random());
        auto candidate = TemporaryPathFor(destination, suffix);
        std::error_code error;
        const auto exists = std::filesystem::exists(candidate, error);
        if (error) {
            return std::unexpected(error);
        }
        if (!exists) {
            return candidate;
        }
    }
    return std::unexpected(std::make_error_code(std::errc::file_exists));
}

[[nodiscard]] std::error_code
CommitTemporaryFile(const std::filesystem::path &temporary,
                    const std::filesystem::path &destination,
                    bool replace_existing) {
#if defined(_WIN32)
    DWORD flags = MOVEFILE_WRITE_THROUGH;
    if (replace_existing) {
        flags |= MOVEFILE_REPLACE_EXISTING;
    }
    if (MoveFileExW(temporary.c_str(), destination.c_str(), flags) != 0) {
        return {};
    }
    return {static_cast<int>(GetLastError()), std::system_category()};
#else
    std::error_code error;
    if (replace_existing) {
        std::filesystem::rename(temporary, destination, error);
        return error;
    }

    std::filesystem::create_hard_link(temporary, destination, error);
    if (error) {
        return error;
    }
    static_cast<void>(std::filesystem::remove(temporary, error));
    return {};
#endif
}

[[nodiscard]] std::unexpected<DocumentExportFailure>
FilesystemFailure(const ExportDocumentRequest &request,
                  DocumentExportFailureKind kind, std::error_code error) {
    if (!error) {
        error = std::make_error_code(std::errc::io_error);
    }
    return std::unexpected(DocumentExportFailure{
        .path = request.path,
        .kind = kind,
        .filesystem_error = error,
        .diagnostics = {},
    });
}

} // namespace

DocumentExportService::DocumentExportService(
    const core::FormatRegistry &registry) noexcept
    : pipeline_(registry) {}

ExportDocumentResult
DocumentExportService::ExportDocument(ExportDocumentRequest request) const {
    if (!core::IsValidTimelineEventProjection(request.event_projection)) {
        return std::unexpected(DocumentExportFailure{
            .path = std::move(request.path),
            .kind = DocumentExportFailureKind::kInvalidRequest,
            .filesystem_error =
                std::make_error_code(std::errc::invalid_argument),
            .diagnostics = {},
        });
    }

    std::error_code exists_error;
    const auto destination_exists =
        std::filesystem::exists(request.path, exists_error);
    if (exists_error) {
        return FilesystemFailure(
            request, DocumentExportFailureKind::kWriteFailed, exists_error);
    }
    if (destination_exists && !request.replace_existing) {
        return FilesystemFailure(request,
                                 DocumentExportFailureKind::kDestinationExists,
                                 std::make_error_code(std::errc::file_exists));
    }

    auto export_result = pipeline_.Export(
        core::ExportRequest{
            .document = request.document,
            .event_projection = std::move(request.event_projection),
            .options = std::move(request.options),
        },
        request.format_identifier);
    if (!export_result.artifact.has_value()) {
        return std::unexpected(DocumentExportFailure{
            .path = std::move(request.path),
            .kind = DocumentExportFailureKind::kExportFailed,
            .filesystem_error = {},
            .diagnostics = std::move(export_result.diagnostics),
        });
    }

    auto temporary_path = AvailableTemporaryPath(request.path);
    if (!temporary_path.has_value()) {
        return FilesystemFailure(request,
                                 DocumentExportFailureKind::kWriteFailed,
                                 temporary_path.error());
    }
    const TemporaryFile temporary{std::move(*temporary_path)};

    errno = 0;
    std::ofstream output{
        temporary.path(),
        std::ios::binary | std::ios::out | std::ios::trunc,
    };
    if (!output.is_open()) {
        return FilesystemFailure(
            request, DocumentExportFailureKind::kWriteFailed, IoError());
    }

    const auto &content = export_result.artifact->content;
    if (content.size() >
        static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        return FilesystemFailure(
            request, DocumentExportFailureKind::kWriteFailed,
            std::make_error_code(std::errc::value_too_large));
    }
    output.write(reinterpret_cast<const char *>(content.data()),
                 static_cast<std::streamsize>(content.size()));
    output.close();
    if (!output) {
        return FilesystemFailure(
            request, DocumentExportFailureKind::kWriteFailed, IoError());
    }

    const auto commit_error = CommitTemporaryFile(
        temporary.path(), request.path, request.replace_existing);
    if (commit_error) {
        const auto kind =
            !request.replace_existing && commit_error == std::errc::file_exists
                ? DocumentExportFailureKind::kDestinationExists
                : DocumentExportFailureKind::kCommitFailed;
        return FilesystemFailure(request, kind, commit_error);
    }

    return DocumentExportReceipt{
        .path = std::move(request.path),
        .diagnostics = std::move(export_result.diagnostics),
    };
}

} // namespace edit_atlas::services
