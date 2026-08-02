#include <edit_atlas/app/timeline_document_workflow.hpp>

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>

#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrentRun>

#include <utility>

namespace edit_atlas::app {

TimelineDocumentWorkflow::TimelineDocumentWorkflow(
    const core::FormatRegistry &registry, QObject *parent)
    : QObject{parent}, export_service_{registry}, import_service_{registry} {
    connect(&export_watcher_,
            &QFutureWatcher<services::TimelineDocumentExportResult>::finished,
            this, &TimelineDocumentWorkflow::ExportFinished);
    connect(&import_watcher_,
            &QFutureWatcher<services::TimelineDocumentImportResult>::finished,
            this, &TimelineDocumentWorkflow::ImportFinished);
}

TimelineDocumentWorkflow::~TimelineDocumentWorkflow(void) {
    if (export_watcher_.isRunning()) {
        export_watcher_.waitForFinished();
    }
    if (import_watcher_.isRunning()) {
        import_watcher_.waitForFinished();
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
    return export_watcher_.isRunning() || import_watcher_.isRunning();
}

bool TimelineDocumentWorkflow::IsExporting(void) const noexcept {
    return export_watcher_.isRunning();
}

} // namespace edit_atlas::app
