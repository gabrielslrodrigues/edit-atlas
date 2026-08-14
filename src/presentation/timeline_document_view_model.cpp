#include <edit_atlas/presentation/timeline_document_view_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/presentation/timeline_document_workflow.hpp>
#include <edit_atlas/presentation/timeline_event_model.hpp>

#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>

#include <QObject>
#include <QtGlobal>

#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace edit_atlas::presentation {

TimelineDocumentViewModel::TimelineDocumentViewModel(
    const core::FormatRegistry &registry, QObject *parent)
    : QObject{parent}, workflow_{registry}, event_model_{},
      event_projection_{core::DefaultTimelineEventProjection().begin(),
                        core::DefaultTimelineEventProjection().end()} {
    connect(&workflow_, &TimelineDocumentWorkflow::ImportFinished, this,
            &TimelineDocumentViewModel::HandleImportFinished);
    connect(&workflow_, &TimelineDocumentWorkflow::RenderedVideoExportFinished,
            this, &TimelineDocumentViewModel::HandleExportFinished);
    connect(&workflow_,
            &TimelineDocumentWorkflow::FrameExtractionProgressChanged, this,
            [this](qulonglong completed, qulonglong total) {
                extracted_frame_count_ = completed;
                total_frame_count_ = total;
                emit FrameExtractionProgressChanged(completed, total);
            });
}

TimelineDocumentCommandResult TimelineDocumentViewModel::Import(
    services::TimelineDocumentImportRequest request) {
    if (IsBusy()) {
        return std::unexpected{TimelineDocumentCommandError::kBusy};
    }

    ResetDocument();
    source_path_ = request.path;
    SetDocumentState(TimelineDocumentState::kImporting);
    workflow_.Import(std::move(request));
    return {};
}

TimelineDocumentCommandResult TimelineDocumentViewModel::Clear(void) {
    if (IsBusy()) {
        return std::unexpected{TimelineDocumentCommandError::kBusy};
    }
    ResetDocument();
    SetDocumentState(TimelineDocumentState::kEmpty);
    return {};
}

void TimelineDocumentViewModel::SetFilterQuery(
    services::TimelineFilterQuery query) {
    if (filter_query_ == query) {
        return;
    }
    filter_query_ = std::move(query);
    ApplyFilter();
}

TimelineDocumentCommandResult TimelineDocumentViewModel::SetEventProjection(
    std::vector<core::TimelineEventField> projection) {
    if (!core::IsValidTimelineEventProjection(projection)) {
        return std::unexpected{
            TimelineDocumentCommandError::kInvalidProjection};
    }
    if (event_projection_ == projection) {
        return {};
    }
    event_projection_ = std::move(projection);
    emit EventProjectionChanged();
    return {};
}

TimelineDocumentCommandResult
TimelineDocumentViewModel::Export(TimelineExportRequest request) {
    if (IsBusy()) {
        return std::unexpected{TimelineDocumentCommandError::kBusy};
    }
    if (!document_.has_value()) {
        return std::unexpected{TimelineDocumentCommandError::kNoDocument};
    }
    if (filter_error_.has_value()) {
        return std::unexpected{TimelineDocumentCommandError::kInvalidFilter};
    }
    if (!core::IsValidTimelineEventProjection(event_projection_)) {
        return std::unexpected{
            TimelineDocumentCommandError::kInvalidProjection};
    }

    services::TimelineDocumentExportRequest document_export{
        .path = std::move(request.path),
        .format_identifier = std::move(request.format_identifier),
        .timeline =
            services::SelectTimelineEvents(*document_, event_selection_),
        .event_projection = event_projection_,
        .options = std::move(request.options),
        .event_images = {},
        .replace_existing = request.replace_existing,
    };
    export_result_.reset();
    extracted_frame_count_ = 0;
    total_frame_count_ = 0;
    SetExportState(TimelineExportState::kExporting);
    workflow_.ExportWithRenderedVideo(
        services::TimelineRenderedVideoExportRequest{
            .document_export = std::move(document_export),
            .reference_timeline = *document_,
            .video_path = std::move(request.video_path),
        });
    return {};
}

void TimelineDocumentViewModel::CancelExport(void) {
    workflow_.CancelRenderedVideoExport();
}

TimelineDocumentState
TimelineDocumentViewModel::DocumentState(void) const noexcept {
    return document_state_;
}

TimelineExportState
TimelineDocumentViewModel::ExportState(void) const noexcept {
    return export_state_;
}

bool TimelineDocumentViewModel::IsBusy(void) const noexcept {
    return document_state_ == TimelineDocumentState::kImporting ||
           export_state_ == TimelineExportState::kExporting ||
           workflow_.IsBusy();
}

bool TimelineDocumentViewModel::CanExport(void) const noexcept {
    return document_state_ == TimelineDocumentState::kReady && !IsBusy() &&
           !filter_error_.has_value() &&
           core::IsValidTimelineEventProjection(event_projection_);
}

const std::filesystem::path &
TimelineDocumentViewModel::SourcePath(void) const noexcept {
    return source_path_;
}

const core::TimelineDocument *
TimelineDocumentViewModel::Document(void) const noexcept {
    return document_.has_value() ? &*document_ : nullptr;
}

std::span<const core::Diagnostic>
TimelineDocumentViewModel::ImportDiagnostics(void) const noexcept {
    return import_diagnostics_;
}

const services::TimelineDocumentImportFailure *
TimelineDocumentViewModel::ImportFailure(void) const noexcept {
    return import_failure_.has_value() ? &*import_failure_ : nullptr;
}

const services::TimelineFilterQuery &
TimelineDocumentViewModel::FilterQuery(void) const noexcept {
    return filter_query_;
}

std::span<const std::size_t>
TimelineDocumentViewModel::EventSelection(void) const noexcept {
    return event_selection_;
}

const services::TimelineFilterError *
TimelineDocumentViewModel::FilterError(void) const noexcept {
    return filter_error_.has_value() ? &*filter_error_ : nullptr;
}

std::span<const core::TimelineEventField>
TimelineDocumentViewModel::EventProjection(void) const noexcept {
    return event_projection_;
}

TimelineEventModel &TimelineDocumentViewModel::EventModel(void) noexcept {
    return event_model_;
}

const TimelineEventModel &
TimelineDocumentViewModel::EventModel(void) const noexcept {
    return event_model_;
}

const services::TimelineRenderedVideoExportResult *
TimelineDocumentViewModel::ExportResult(void) const noexcept {
    return export_result_.has_value() ? &*export_result_ : nullptr;
}

qulonglong TimelineDocumentViewModel::ExtractedFrameCount(void) const noexcept {
    return extracted_frame_count_;
}

qulonglong TimelineDocumentViewModel::TotalFrameCount(void) const noexcept {
    return total_frame_count_;
}

void TimelineDocumentViewModel::ApplyFilter(void) {
    if (!document_.has_value()) {
        event_selection_.clear();
        filter_error_.reset();
        event_model_.SetEventSelection({});
        emit FilterChanged();
        return;
    }

    auto result = services::FilterTimelineEvents(*document_, filter_query_);
    if (!result.has_value()) {
        event_selection_.clear();
        filter_error_ = std::move(result.error());
    } else {
        event_selection_ = std::move(*result);
        filter_error_.reset();
    }
    event_model_.SetEventSelection(event_selection_);
    emit FilterChanged();
}

void TimelineDocumentViewModel::HandleImportFinished(void) {
    auto result = workflow_.ImportResult();
    if (!result.has_value()) {
        source_path_ = result.error().path;
        import_failure_ = std::move(result.error());
        SetDocumentState(TimelineDocumentState::kImportFailed);
        emit DocumentChanged();
        return;
    }

    source_path_ = std::move(result->path);
    document_ = std::move(result->timeline);
    import_diagnostics_ = std::move(result->diagnostics);
    event_model_.SetDocument(&*document_);
    ApplyFilter();
    SetDocumentState(TimelineDocumentState::kReady);
    emit DocumentChanged();
}

void TimelineDocumentViewModel::HandleExportFinished(void) {
    export_result_ = workflow_.RenderedVideoExportResult();
    SetExportState(TimelineExportState::kIdle);
    emit ExportFinished();
}

void TimelineDocumentViewModel::ResetDocument(void) {
    source_path_.clear();
    document_.reset();
    import_diagnostics_.clear();
    import_failure_.reset();
    event_selection_.clear();
    filter_error_.reset();
    export_result_.reset();
    extracted_frame_count_ = 0;
    total_frame_count_ = 0;
    event_model_.SetDocument(nullptr);
    emit FilterChanged();
    emit DocumentChanged();
}

void TimelineDocumentViewModel::SetDocumentState(TimelineDocumentState state) {
    if (document_state_ == state) {
        return;
    }
    document_state_ = state;
    emit DocumentStateChanged();
}

void TimelineDocumentViewModel::SetExportState(TimelineExportState state) {
    if (export_state_ == state) {
        return;
    }
    export_state_ = state;
    emit ExportStateChanged();
}

} // namespace edit_atlas::presentation
