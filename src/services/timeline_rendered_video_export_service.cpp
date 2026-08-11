#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>

#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace edit_atlas::services {
namespace {

[[nodiscard]] core::Diagnostic VideoRequiredDiagnostic(void) {
    return core::Diagnostic{
        .severity = core::DiagnosticSeverity::kError,
        .code =
            std::string{
                timeline_rendered_video_export_diagnostic_code::kVideoRequired},
        .message =
            "A validated rendered video is required when the Initial Frame "
            "field is selected.",
        .location = std::nullopt,
    };
}

[[nodiscard]] core::Diagnostic CancelledDiagnostic(void) {
    return core::Diagnostic{
        .severity = core::DiagnosticSeverity::kError,
        .code =
            std::string{timeline_frame_extraction_diagnostic_code::kCancelled},
        .message = "Initial-frame extraction was cancelled.",
        .location = std::nullopt,
    };
}

[[nodiscard]] bool
RequiresInitialFrames(const TimelineDocumentExportRequest &request) noexcept {
    return std::ranges::contains(request.event_projection,
                                 core::TimelineEventField::kInitialFrame);
}

[[nodiscard]] TimelineRenderedVideoExportFailure
InspectionFailure(TimelineVideoInspectionFailure failure) {
    return TimelineRenderedVideoExportFailure{
        .kind = TimelineRenderedVideoExportFailureKind::kVideoInspectionFailed,
        .video_path = std::move(failure.path),
        .decoder_failure = std::move(failure.decoder_failure),
        .document_export_failure = std::nullopt,
        .diagnostics = std::move(failure.diagnostics),
    };
}

[[nodiscard]] TimelineRenderedVideoExportFailure
ExtractionFailure(const std::filesystem::path &path,
                  TimelineFrameExtractionFailure failure) {
    return TimelineRenderedVideoExportFailure{
        .kind = TimelineRenderedVideoExportFailureKind::kFrameExtractionFailed,
        .video_path = path,
        .decoder_failure = std::move(failure.decoder_failure),
        .document_export_failure = std::nullopt,
        .diagnostics = std::move(failure.diagnostics),
    };
}

[[nodiscard]] TimelineRenderedVideoExportFailure
DocumentExportFailure(TimelineDocumentExportFailure failure) {
    auto diagnostics = failure.diagnostics;
    return TimelineRenderedVideoExportFailure{
        .kind = TimelineRenderedVideoExportFailureKind::kDocumentExportFailed,
        .video_path = std::nullopt,
        .decoder_failure = std::nullopt,
        .document_export_failure = std::move(failure),
        .diagnostics = std::move(diagnostics),
    };
}

} // namespace

TimelineRenderedVideoExportService::TimelineRenderedVideoExportService(
    const core::FormatRegistry &registry) noexcept
    : document_export_service_{registry} {}

TimelineRenderedVideoExportResult TimelineRenderedVideoExportService::Export(
    TimelineRenderedVideoExportRequest request, std::stop_token stop_token,
    const TimelineFrameExtractionProgressCallback &progress) const {
    std::optional<media::VideoMediaInfo> video_information;
    std::optional<TimelineVideoMapping> video_mapping;
    std::size_t unique_frame_count = 0;
    if (RequiresInitialFrames(request.document_export)) {
        if (!request.video_path.has_value()) {
            return std::unexpected(TimelineRenderedVideoExportFailure{
                .kind = TimelineRenderedVideoExportFailureKind::kVideoRequired,
                .video_path = std::nullopt,
                .decoder_failure = std::nullopt,
                .document_export_failure = std::nullopt,
                .diagnostics = {VideoRequiredDiagnostic()},
            });
        }
        if (stop_token.stop_requested()) {
            return std::unexpected(TimelineRenderedVideoExportFailure{
                .kind = TimelineRenderedVideoExportFailureKind::
                    kFrameExtractionFailed,
                .video_path = request.video_path,
                .decoder_failure = std::nullopt,
                .document_export_failure = std::nullopt,
                .diagnostics = {CancelledDiagnostic()},
            });
        }

        auto inspection = video_inspection_service_.Inspect(
            *request.video_path, request.reference_timeline);
        if (!inspection.has_value()) {
            return std::unexpected(
                InspectionFailure(std::move(inspection.error())));
        }
        video_information = inspection->decoder->Information();
        video_mapping = inspection->mapping;
        auto extraction = frame_extraction_service_.Extract(
            request.document_export.timeline, std::move(*inspection),
            TimelineFrameExtractionOptions{
                .frame_output =
                    media::VideoFrameOutputOptions{
                        .size_limit =
                            media::VideoFrameSizeLimit{
                                .maximum_width =
                                    formats::xlsx::kInitialFrameMaximumWidth,
                                .maximum_height =
                                    formats::xlsx::kInitialFrameMaximumHeight,
                            },
                    },
            },
            stop_token, progress);
        if (!extraction.has_value()) {
            return std::unexpected(ExtractionFailure(
                *request.video_path, std::move(extraction.error())));
        }
        if (stop_token.stop_requested()) {
            return std::unexpected(TimelineRenderedVideoExportFailure{
                .kind = TimelineRenderedVideoExportFailureKind::
                    kFrameExtractionFailed,
                .video_path = request.video_path,
                .decoder_failure = std::nullopt,
                .document_export_failure = std::nullopt,
                .diagnostics = {CancelledDiagnostic()},
            });
        }
        unique_frame_count = extraction->unique_frame_count;
        request.document_export.event_images.clear();
        request.document_export.event_images.reserve(extraction->frames.size());
        for (auto &frame : extraction->frames) {
            request.document_export.event_images.push_back(
                core::TimelineEventImage{
                    .event_index = frame.event_index,
                    .image = std::move(frame.image),
                });
        }
    }

    auto document_export =
        document_export_service_.Export(std::move(request.document_export));
    if (!document_export.has_value()) {
        return std::unexpected(
            DocumentExportFailure(std::move(document_export.error())));
    }
    return TimelineRenderedVideoExportReceipt{
        .document_export = std::move(*document_export),
        .video_information = std::move(video_information),
        .video_mapping = std::move(video_mapping),
        .unique_frame_count = unique_frame_count,
    };
}

} // namespace edit_atlas::services
