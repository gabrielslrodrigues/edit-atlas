#include <edit_atlas/services/timeline_document_export_service.hpp>

#include <edit_atlas/core/format.hpp>

#include <edit_atlas/storage/local_file.hpp>

#include <utility>

namespace edit_atlas::services {
namespace {

[[nodiscard]] std::unexpected<TimelineDocumentExportFailure>
FilesystemFailure(const TimelineDocumentExportRequest &request,
                  const storage::LocalFileFailure &failure) {
    auto kind = TimelineDocumentExportFailureKind::kWriteFailed;
    if (failure.kind == storage::LocalFileFailureKind::kDestinationExists) {
        kind = TimelineDocumentExportFailureKind::kDestinationExists;
    } else if (failure.kind == storage::LocalFileFailureKind::kCommitFailed) {
        kind = TimelineDocumentExportFailureKind::kCommitFailed;
    }
    return std::unexpected(TimelineDocumentExportFailure{
        .path = request.path,
        .kind = kind,
        .filesystem_error = failure.filesystem_error,
        .diagnostics = {},
    });
}

} // namespace

TimelineDocumentExportService::TimelineDocumentExportService(
    const core::FormatRegistry &registry) noexcept
    : pipeline_(registry) {}

TimelineDocumentExportResult TimelineDocumentExportService::Export(
    TimelineDocumentExportRequest request) const {
    if (!core::IsValidTimelineEventProjection(request.event_projection)) {
        return std::unexpected(TimelineDocumentExportFailure{
            .path = std::move(request.path),
            .kind = TimelineDocumentExportFailureKind::kInvalidRequest,
            .filesystem_error =
                std::make_error_code(std::errc::invalid_argument),
            .diagnostics = {},
        });
    }

    auto export_result = pipeline_.Export(
        core::ExportRequest{
            .document = request.timeline,
            .event_projection = std::move(request.event_projection),
            .options = std::move(request.options),
            .event_images = std::move(request.event_images),
        },
        request.format_identifier);
    if (!export_result.artifact.has_value()) {
        return std::unexpected(TimelineDocumentExportFailure{
            .path = std::move(request.path),
            .kind = TimelineDocumentExportFailureKind::kExportFailed,
            .filesystem_error = {},
            .diagnostics = std::move(export_result.diagnostics),
        });
    }

    const auto &content = export_result.artifact->content;
    const auto existing_file_policy =
        request.replace_existing ? storage::ExistingFilePolicy::kReplace
                                 : storage::ExistingFilePolicy::kPreserve;
    const auto write_result = storage::WriteLocalFileAtomically(
        request.path, content, existing_file_policy);
    if (!write_result.has_value()) {
        return FilesystemFailure(request, write_result.error());
    }

    return TimelineDocumentExportReceipt{
        .path = std::move(request.path),
        .diagnostics = std::move(export_result.diagnostics),
    };
}

} // namespace edit_atlas::services
