#include <edit_atlas/presentation/timeline_document_workflow.hpp>

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>
#include <edit_atlas/services/timeline_frame_extraction_service.hpp>
#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>

#include <QFutureWatcher>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <Qt>
#include <QtConcurrentRun>

#include <utility>

namespace edit_atlas::presentation {

TimelineDocumentWorkflow::TimelineDocumentWorkflow(
    const core::FormatRegistry &registry, QObject *parent)
    : QObject{parent}, export_service_{registry}, import_service_{registry},
      rendered_video_export_service_{registry} {
    connect(&export_watcher_,
            &QFutureWatcher<services::TimelineDocumentExportResult>::finished,
            this, &TimelineDocumentWorkflow::ExportFinished);
    connect(&import_watcher_,
            &QFutureWatcher<services::TimelineDocumentImportResult>::finished,
            this, &TimelineDocumentWorkflow::ImportFinished);
    connect(
        &rendered_video_export_watcher_,
        &QFutureWatcher<services::TimelineRenderedVideoExportResult>::finished,
        this, &TimelineDocumentWorkflow::RenderedVideoExportFinished);
}

TimelineDocumentWorkflow::~TimelineDocumentWorkflow(void) {
    rendered_video_export_stop_source_.request_stop();
    if (export_watcher_.isRunning()) {
        export_watcher_.waitForFinished();
    }
    if (import_watcher_.isRunning()) {
        import_watcher_.waitForFinished();
    }
    if (rendered_video_export_watcher_.isRunning()) {
        rendered_video_export_watcher_.waitForFinished();
    }
}

void TimelineDocumentWorkflow::Export(
    services::TimelineDocumentExportRequest request) {
    const auto *service = &export_service_;
    export_watcher_.setFuture(QtConcurrent::run(
        [service, request = std::move(request)](void) mutable {
            return service->Export(std::move(request));
        }));
}

services::TimelineDocumentExportResult
TimelineDocumentWorkflow::ExportResult(void) const {
    return export_watcher_.result();
}

void TimelineDocumentWorkflow::ExportWithRenderedVideo(
    services::TimelineRenderedVideoExportRequest request) {
    rendered_video_export_stop_source_ = std::stop_source{};
    const auto stop_token = rendered_video_export_stop_source_.get_token();
    const auto *service = &rendered_video_export_service_;
    QPointer<TimelineDocumentWorkflow> self{this};
    rendered_video_export_watcher_.setFuture(
        QtConcurrent::run([service, self, stop_token,
                           request = std::move(request)](void) mutable {
            return service->Export(
                std::move(request), stop_token,
                [self](
                    const services::TimelineFrameExtractionProgress &progress) {
                    if (self == nullptr) {
                        return;
                    }
                    QMetaObject::invokeMethod(
                        self,
                        [self, progress](void) {
                            if (self != nullptr) {
                                emit self->FrameExtractionProgressChanged(
                                    static_cast<qulonglong>(
                                        progress.completed_events),
                                    static_cast<qulonglong>(
                                        progress.total_events));
                            }
                        },
                        Qt::QueuedConnection);
                });
        }));
}

services::TimelineRenderedVideoExportResult
TimelineDocumentWorkflow::RenderedVideoExportResult(void) const {
    return rendered_video_export_watcher_.result();
}

void TimelineDocumentWorkflow::CancelRenderedVideoExport(void) {
    rendered_video_export_stop_source_.request_stop();
}

void TimelineDocumentWorkflow::Import(
    services::TimelineDocumentImportRequest request) {
    const auto *service = &import_service_;
    import_watcher_.setFuture(QtConcurrent::run(
        [service, request = std::move(request)](void) mutable {
            return service->Import(std::move(request));
        }));
}

services::TimelineDocumentImportResult
TimelineDocumentWorkflow::ImportResult(void) const {
    return import_watcher_.result();
}

bool TimelineDocumentWorkflow::IsBusy(void) const noexcept {
    return export_watcher_.isRunning() || import_watcher_.isRunning() ||
           rendered_video_export_watcher_.isRunning();
}

bool TimelineDocumentWorkflow::IsExporting(void) const noexcept {
    return export_watcher_.isRunning() ||
           rendered_video_export_watcher_.isRunning();
}

} // namespace edit_atlas::presentation
