#ifndef EDIT_ATLAS_FRONTENDS_QUICK_SPREADSHEET_EXPORT_VIEW_MODEL_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_SPREADSHEET_EXPORT_VIEW_MODEL_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/presentation/timeline_document_view_model.hpp>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <QtQmlIntegration/qqmlintegration.h>

#include <string>

namespace edit_atlas::frontends::quick {

/// Adapts spreadsheet-export state and commands for the Qt Quick frontend.
class SpreadsheetExportViewModel final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(SpreadsheetExport)
    QML_UNCREATABLE("Provided by the Edit Atlas application")

    Q_PROPERTY(bool available READ IsAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(bool busy READ IsBusy NOTIFY busyChanged)
    Q_PROPERTY(bool renderedVideoRequired READ IsRenderedVideoRequired NOTIFY
                   renderedVideoRequirementChanged)
    Q_PROPERTY(qulonglong completedFrameCount READ CompletedFrameCount NOTIFY
                   progressChanged)
    Q_PROPERTY(
        qulonglong totalFrameCount READ TotalFrameCount NOTIFY progressChanged)
    Q_PROPERTY(QUrl suggestedDestinationUrl READ SuggestedDestinationUrl NOTIFY
                   suggestedDestinationChanged)
    Q_PROPERTY(QUrl suggestedVideoFolderUrl READ SuggestedVideoFolderUrl NOTIFY
                   suggestedDestinationChanged)
    Q_PROPERTY(QString resultPath READ ResultPath NOTIFY resultChanged)
    Q_PROPERTY(
        QString resultDetailsText READ ResultDetailsText NOTIFY resultChanged)
    Q_PROPERTY(QString warningText READ WarningText NOTIFY resultChanged)
    Q_PROPERTY(QString errorText READ ErrorText NOTIFY resultChanged)
    Q_PROPERTY(bool hasWarnings READ HasWarnings NOTIFY resultChanged)

  public:
    /// Creates an export adapter over the shared document ViewModel.
    SpreadsheetExportViewModel(
        const core::FormatRegistry &registry,
        presentation::TimelineDocumentViewModel &document_view_model,
        QObject *parent = nullptr);
    /// Destroys the adapter after the shared export workflow has stopped.
    ~SpreadsheetExportViewModel(void) override = default;

    /// Export adapters are non-copyable QObject owners.
    SpreadsheetExportViewModel(const SpreadsheetExportViewModel &) = delete;
    /// Export adapters are non-copy-assignable QObject owners.
    SpreadsheetExportViewModel &
    operator=(const SpreadsheetExportViewModel &) = delete;
    /// Export adapters are non-movable QObject owners.
    SpreadsheetExportViewModel(SpreadsheetExportViewModel &&) = delete;
    /// Export adapters are non-move-assignable QObject owners.
    SpreadsheetExportViewModel &
    operator=(SpreadsheetExportViewModel &&) = delete;

    /// Returns whether the current document can be exported as XLSX.
    [[nodiscard]] bool IsAvailable(void) const noexcept;
    /// Returns whether an export operation is running.
    [[nodiscard]] bool IsBusy(void) const noexcept;
    /// Returns whether the active projection requires a rendered video.
    [[nodiscard]] bool IsRenderedVideoRequired(void) const noexcept;
    /// Returns the number of distinct initial frames extracted so far.
    [[nodiscard]] qulonglong CompletedFrameCount(void) const noexcept;
    /// Returns the number of distinct initial frames to extract.
    [[nodiscard]] qulonglong TotalFrameCount(void) const noexcept;
    /// Returns the preferred XLSX destination for the active timeline.
    [[nodiscard]] QUrl SuggestedDestinationUrl(void) const;
    /// Returns the preferred folder for selecting a rendered video.
    [[nodiscard]] QUrl SuggestedVideoFolderUrl(void) const;
    /// Returns the completed export path for presentation.
    [[nodiscard]] QString ResultPath(void) const;
    /// Returns optional rendered-video details for a successful export.
    [[nodiscard]] QString ResultDetailsText(void) const;
    /// Returns localized non-fatal exporter diagnostics.
    [[nodiscard]] QString WarningText(void) const;
    /// Returns localized details for the latest export failure.
    [[nodiscard]] QString ErrorText(void) const;
    /// Returns whether the completed workbook contains warnings.
    [[nodiscard]] bool HasWarnings(void) const noexcept;

    /// Starts an XLSX export using frontend-selected presentation options.
    Q_INVOKABLE bool Start(const QUrl &destination,
                           const QString &workbook_language,
                           bool include_timeline_sheet,
                           bool include_diagnostics_sheet,
                           const QUrl &rendered_video);
    /// Requests cooperative cancellation of rendered-video frame extraction.
    Q_INVOKABLE void Cancel(void);
    /// Opens the completed workbook's containing folder.
    Q_INVOKABLE bool RevealResult(void) const;
    /// Clears the latest completed result before a new export interaction.
    Q_INVOKABLE void ClearResult(void);

  signals:
    /// Reports changed export command availability.
    void availabilityChanged(void);
    /// Reports that export activity started or stopped.
    void busyChanged(void);
    /// Reports that the projection added or removed the Initial Frame field.
    void renderedVideoRequirementChanged(void);
    /// Reports updated frame-extraction progress.
    void progressChanged(void);
    /// Reports changed export result presentation.
    void resultChanged(void);
    /// Reports a successful export result.
    void exportSucceeded(void);
    /// Reports a failed export result.
    void exportFailed(void);
    /// Reports cancellation without a completed workbook.
    void exportCancelled(void);
    /// Reports that the suggested export or video folder changed.
    void suggestedDestinationChanged(void);

  private:
    void HandleExportFinished(void);
    void SetImmediateFailure(QString error);

    presentation::TimelineDocumentViewModel &document_view_model_;
    std::string exporter_identifier_;
    QString result_path_;
    QString result_details_text_;
    QString warning_text_;
    QString error_text_;
};

} // namespace edit_atlas::frontends::quick

#endif // EDIT_ATLAS_FRONTENDS_QUICK_SPREADSHEET_EXPORT_VIEW_MODEL_HPP_
