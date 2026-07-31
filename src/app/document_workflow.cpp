#include <edit_atlas/app/document_workflow.hpp>

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/document_export_service.hpp>
#include <edit_atlas/services/document_import_service.hpp>

#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrentRun>

#include <utility>

namespace edit_atlas::app {

DocumentWorkflow::DocumentWorkflow(const core::FormatRegistry &registry,
                                   QObject *parent)
    : QObject{parent}, export_service_{registry}, import_service_{registry} {
    connect(&export_watcher_,
            &QFutureWatcher<services::ExportDocumentResult>::finished, this,
            &DocumentWorkflow::ExportFinished);
    connect(&import_watcher_,
            &QFutureWatcher<services::ImportDocumentResult>::finished, this,
            &DocumentWorkflow::ImportFinished);
}

DocumentWorkflow::~DocumentWorkflow(void) {
    if (export_watcher_.isRunning()) {
        export_watcher_.waitForFinished();
    }
    if (import_watcher_.isRunning()) {
        import_watcher_.waitForFinished();
    }
}

void DocumentWorkflow::Export(services::ExportDocumentRequest request) {
    const auto *service = &export_service_;
    export_watcher_.setFuture(QtConcurrent::run(
        [service, request = std::move(request)](void) mutable {
            return service->ExportDocument(std::move(request));
        }));
}

services::ExportDocumentResult DocumentWorkflow::ExportResult(void) const {
    return export_watcher_.result();
}

void DocumentWorkflow::Import(services::ImportDocumentRequest request) {
    const auto *service = &import_service_;
    import_watcher_.setFuture(QtConcurrent::run(
        [service, request = std::move(request)](void) mutable {
            return service->ImportDocument(std::move(request));
        }));
}

services::ImportDocumentResult DocumentWorkflow::ImportResult(void) const {
    return import_watcher_.result();
}

bool DocumentWorkflow::IsBusy(void) const noexcept {
    return export_watcher_.isRunning() || import_watcher_.isRunning();
}

bool DocumentWorkflow::IsExporting(void) const noexcept {
    return export_watcher_.isRunning();
}

} // namespace edit_atlas::app
