#ifndef EDIT_ATLAS_APP_TIMELINE_DOCUMENT_WORKFLOW_HPP_
#define EDIT_ATLAS_APP_TIMELINE_DOCUMENT_WORKFLOW_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>

#include <QFutureWatcher>
#include <QObject>

namespace edit_atlas::app {

/// Owns Qt asynchronous execution for the UI-independent document services.
class TimelineDocumentWorkflow final : public QObject {
    Q_OBJECT

  public:
    explicit TimelineDocumentWorkflow(const core::FormatRegistry &registry,
                                      QObject *parent = nullptr);
    ~TimelineDocumentWorkflow(void) override;

    TimelineDocumentWorkflow(const TimelineDocumentWorkflow &) = delete;
    TimelineDocumentWorkflow &
    operator=(const TimelineDocumentWorkflow &) = delete;
    TimelineDocumentWorkflow(TimelineDocumentWorkflow &&) = delete;
    TimelineDocumentWorkflow &operator=(TimelineDocumentWorkflow &&) = delete;

    void Export(services::TimelineDocumentExportRequest request);
    [[nodiscard]] services::TimelineDocumentExportResult
    ExportResult(void) const;
    void Import(services::TimelineDocumentImportRequest request);
    [[nodiscard]] services::TimelineDocumentImportResult
    ImportResult(void) const;
    [[nodiscard]] bool IsBusy(void) const noexcept;
    [[nodiscard]] bool IsExporting(void) const noexcept;

  signals:
    void ExportFinished(void);
    void ImportFinished(void);

  private:
    services::TimelineDocumentExportService export_service_;
    services::TimelineDocumentImportService import_service_;
    QFutureWatcher<services::TimelineDocumentExportResult> export_watcher_;
    QFutureWatcher<services::TimelineDocumentImportResult> import_watcher_;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_TIMELINE_DOCUMENT_WORKFLOW_HPP_
