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
    /// Starts rendered-video preparation and export on a worker thread.
    void ExportWithRenderedVideo(
        services::TimelineRenderedVideoExportRequest request);
    /// Returns the completed rendered-video-aware export result.
    [[nodiscard]] services::TimelineRenderedVideoExportResult
    RenderedVideoExportResult(void) const;
    /// Requests cooperative cancellation of initial-frame extraction.
    void CancelRenderedVideoExport(void);
    void Import(services::TimelineDocumentImportRequest request);
    [[nodiscard]] services::TimelineDocumentImportResult
    ImportResult(void) const;
    [[nodiscard]] bool IsBusy(void) const noexcept;
    [[nodiscard]] bool IsExporting(void) const noexcept;

  signals:
    void ExportFinished(void);
    /// Reports extracted and total event-frame counts.
    void FrameExtractionProgressChanged(qulonglong completed_events,
                                        qulonglong total_events);
    void ImportFinished(void);
    /// Reports completion of rendered-video preparation and export.
    void RenderedVideoExportFinished(void);

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
