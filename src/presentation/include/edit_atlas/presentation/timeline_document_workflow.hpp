#ifndef EDIT_ATLAS_PRESENTATION_TIMELINE_DOCUMENT_WORKFLOW_HPP_
#define EDIT_ATLAS_PRESENTATION_TIMELINE_DOCUMENT_WORKFLOW_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>
#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>

#include <QFutureWatcher>
#include <QObject>
#include <QtGlobal>

#include <stop_token>

namespace edit_atlas::presentation {

/// Owns Qt asynchronous execution for the UI-independent document services.
class TimelineDocumentWorkflow final : public QObject {
    Q_OBJECT

  public:
    /// Creates an idle workflow using the supplied format registry.
    explicit TimelineDocumentWorkflow(const core::FormatRegistry &registry,
                                      QObject *parent = nullptr);
    /// Waits for owned asynchronous work before destruction.
    ~TimelineDocumentWorkflow(void) override;

    /// Workflows are non-copyable QObject owners.
    TimelineDocumentWorkflow(const TimelineDocumentWorkflow &) = delete;
    /// Workflows are non-copy-assignable QObject owners.
    TimelineDocumentWorkflow &
    operator=(const TimelineDocumentWorkflow &) = delete;
    /// Workflows are non-movable QObject owners.
    TimelineDocumentWorkflow(TimelineDocumentWorkflow &&) = delete;
    /// Workflows are non-move-assignable QObject owners.
    TimelineDocumentWorkflow &operator=(TimelineDocumentWorkflow &&) = delete;

    /// Starts an ordinary document export on a worker thread.
    void Export(services::TimelineDocumentExportRequest request);
    /// Returns the most recent completed ordinary export result.
    [[nodiscard]] services::TimelineDocumentExportResult
    ExportResult(void) const;
    /// Starts rendered-video preparation and export on a worker thread.
    void ExportWithRenderedVideo(
        services::TimelineRenderedVideoExportRequest request);
    /// Returns the completed rendered-video-aware export result.
    [[nodiscard]] services::TimelineRenderedVideoExportResult
    RenderedVideoExportResult(void) const;
    /// Requests cooperative cancellation of initial-frame extraction.
    void CancelRenderedVideoExport(void);
    /// Starts a document import on a worker thread.
    void Import(services::TimelineDocumentImportRequest request);
    /// Returns the most recent completed import result.
    [[nodiscard]] services::TimelineDocumentImportResult
    ImportResult(void) const;
    /// Returns whether an import or export is running.
    [[nodiscard]] bool IsBusy(void) const noexcept;
    /// Returns whether an ordinary or rendered-video export is running.
    [[nodiscard]] bool IsExporting(void) const noexcept;

  signals:
    /// Reports completion of an ordinary document export.
    void exportFinished(void);
    /// Reports extracted and total event-frame counts.
    void frameExtractionProgressChanged(qulonglong completed_events,
                                        qulonglong total_events);
    /// Reports completion of a document import.
    void importFinished(void);
    /// Reports completion of rendered-video preparation and export.
    void renderedVideoExportFinished(void);

  private:
    services::TimelineDocumentExportService export_service_;
    services::TimelineDocumentImportService import_service_;
    services::TimelineRenderedVideoExportService rendered_video_export_service_;
    QFutureWatcher<services::TimelineDocumentExportResult> export_watcher_;
    QFutureWatcher<services::TimelineDocumentImportResult> import_watcher_;
    QFutureWatcher<services::TimelineRenderedVideoExportResult>
        rendered_video_export_watcher_;
    std::stop_source rendered_video_export_stop_source_;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TIMELINE_DOCUMENT_WORKFLOW_HPP_
