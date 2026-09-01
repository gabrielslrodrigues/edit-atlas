#ifndef EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_SHELL_VIEW_MODEL_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_SHELL_VIEW_MODEL_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/frontends/quick/application_information_view_model.hpp>
#include <edit_atlas/frontends/quick/spreadsheet_export_view_model.hpp>
#include <edit_atlas/frontends/quick/support_bundle_view_model.hpp>
#include <edit_atlas/frontends/quick/timeline_configuration_view_model.hpp>

#include <edit_atlas/presentation/timeline_document_view_model.hpp>
#include <edit_atlas/presentation/translation.hpp>

#include <QAbstractItemModel>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtGlobal>
#include <QtQmlIntegration/qqmlintegration.h>

#include <optional>
#include <string>

class QTranslator;

namespace edit_atlas::frontends::quick {

/// Adapts shared desktop presentation state to the QML application shell.
class ApplicationShellViewModel final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ApplicationShell)
    QML_UNCREATABLE("Provided by the Edit Atlas application")

    Q_PROPERTY(DocumentState documentState READ CurrentDocumentState NOTIFY
                   documentStateChanged)
    Q_PROPERTY(bool empty READ IsEmpty NOTIFY documentStateChanged)
    Q_PROPERTY(bool busy READ IsBusy NOTIFY busyChanged)
    Q_PROPERTY(QString sourceFileName READ SourceFileName NOTIFY
                   documentPresentationChanged)
    Q_PROPERTY(qulonglong eventCount READ EventCount NOTIFY
                   documentPresentationChanged)
    Q_PROPERTY(qulonglong visibleEventCount READ VisibleEventCount NOTIFY
                   documentPresentationChanged)
    Q_PROPERTY(QString timelineTitle READ TimelineTitle NOTIFY
                   documentPresentationChanged)
    Q_PROPERTY(QString timelineSummaryText READ TimelineSummaryText NOTIFY
                   documentPresentationChanged)
    Q_PROPERTY(QAbstractItemModel *eventModel READ EventModel CONSTANT)
    Q_PROPERTY(
        QAbstractItemModel *diagnosticsModel READ DiagnosticsModel CONSTANT)
    Q_PROPERTY(TimelineConfigurationViewModel *timelineConfiguration READ
                   TimelineConfiguration CONSTANT)
    Q_PROPERTY(SpreadsheetExportViewModel *spreadsheetExport READ
                   SpreadsheetExport CONSTANT)
    Q_PROPERTY(ApplicationInformationViewModel *applicationInformation READ
                   ApplicationInformation CONSTANT)
    Q_PROPERTY(
        SupportBundleViewModel *supportBundle READ SupportBundle CONSTANT)
    Q_PROPERTY(int diagnosticCount READ DiagnosticCount NOTIFY
                   documentPresentationChanged)
    Q_PROPERTY(int eventSortColumn READ EventSortColumn NOTIFY eventSortChanged)
    Q_PROPERTY(
        bool eventSortAscending READ EventSortAscending NOTIFY eventSortChanged)
    Q_PROPERTY(QString statusText READ StatusText NOTIFY statusTextChanged)
    Q_PROPERTY(
        QString errorText READ ErrorText NOTIFY documentPresentationChanged)
    Q_PROPERTY(QStringList importFilePatterns READ ImportFilePatterns CONSTANT)
    Q_PROPERTY(
        QStringList recentFiles READ RecentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(bool rememberRecentFiles READ RememberRecentFiles WRITE
                   SetRememberRecentFiles NOTIFY rememberRecentFilesChanged)
    Q_PROPERTY(QString languageCode READ LanguageCode WRITE SetLanguageCode
                   NOTIFY languageChanged)

  public:
    /// Document states rendered by the QML shell.
    enum class DocumentState {
        /// No timeline has been imported.
        kEmpty,
        /// A timeline import is running.
        kImporting,
        /// A timeline is available.
        kReady,
        /// The latest import failed.
        kImportFailed,
    };
    Q_ENUM(DocumentState)

    /// Creates a shell adapter over the shared document ViewModel.
    ApplicationShellViewModel(
        const core::FormatRegistry &registry, QTranslator &translator,
        presentation::ApplicationLanguage initial_language,
        QObject *parent = nullptr);
    /// Destroys the adapter after its document workflow has stopped.
    ~ApplicationShellViewModel(void) override = default;

    ApplicationShellViewModel(const ApplicationShellViewModel &) = delete;
    ApplicationShellViewModel &
    operator=(const ApplicationShellViewModel &) = delete;
    ApplicationShellViewModel(ApplicationShellViewModel &&) = delete;
    ApplicationShellViewModel &operator=(ApplicationShellViewModel &&) = delete;

    /// Returns the shell document state.
    [[nodiscard]] DocumentState CurrentDocumentState(void) const noexcept;
    /// Returns whether no timeline import has been requested.
    [[nodiscard]] bool IsEmpty(void) const noexcept;
    /// Returns whether a document operation prevents new interaction.
    [[nodiscard]] bool IsBusy(void) const noexcept;
    /// Returns the active source file name without its parent directory.
    [[nodiscard]] QString SourceFileName(void) const;
    /// Returns the number of imported timeline events.
    [[nodiscard]] qulonglong EventCount(void) const noexcept;
    /// Returns the number of events currently presented by the item model.
    [[nodiscard]] qulonglong VisibleEventCount(void) const noexcept;
    /// Returns the imported timeline title or source file name.
    [[nodiscard]] QString TimelineTitle(void) const;
    /// Returns localized event-count, frame-rate, and timecode metadata.
    [[nodiscard]] QString TimelineSummaryText(void) const;
    /// Returns the sortable event item model exposed to QML.
    [[nodiscard]] QAbstractItemModel *EventModel(void) noexcept;
    /// Returns the diagnostic item model exposed to QML.
    [[nodiscard]] QAbstractItemModel *DiagnosticsModel(void) noexcept;
    /// Returns editable filtering, template, and projection configuration.
    [[nodiscard]] TimelineConfigurationViewModel *
    TimelineConfiguration(void) noexcept;
    /// Returns the QML-facing spreadsheet-export workflow.
    [[nodiscard]] SpreadsheetExportViewModel *SpreadsheetExport(void) noexcept;
    /// Returns immutable application, runtime, and licensing information.
    [[nodiscard]] ApplicationInformationViewModel *
    ApplicationInformation(void) noexcept;
    /// Returns the QML-facing diagnostic support-bundle workflow.
    [[nodiscard]] SupportBundleViewModel *SupportBundle(void) noexcept;
    /// Returns the number of import diagnostics currently presented.
    [[nodiscard]] int DiagnosticCount(void) const noexcept;
    /// Returns the event column currently used for sorting.
    [[nodiscard]] int EventSortColumn(void) const noexcept;
    /// Returns whether the active event ordering is ascending.
    [[nodiscard]] bool EventSortAscending(void) const noexcept;
    /// Returns a localized status-bar message for the current state.
    [[nodiscard]] QString StatusText(void) const;
    /// Returns localized details for the latest import failure.
    [[nodiscard]] QString ErrorText(void) const;
    /// Returns registered timeline file patterns for the open dialog.
    [[nodiscard]] QStringList ImportFilePatterns(void) const;
    /// Returns recent paths in most-recent-first order.
    [[nodiscard]] QStringList RecentFiles(void) const;
    /// Returns whether recent file paths are persisted.
    [[nodiscard]] bool RememberRecentFiles(void) const;
    /// Enables or disables recent-file persistence.
    void SetRememberRecentFiles(bool enabled);
    /// Returns the stable code for the active interface language.
    [[nodiscard]] QString LanguageCode(void) const;
    /// Changes and persists the interface language.
    void SetLanguageCode(const QString &code);

    /// Imports one local timeline selected by URL.
    Q_INVOKABLE void OpenUrl(const QUrl &url);
    /// Imports one local timeline selected by path.
    Q_INVOKABLE void OpenPath(const QString &path);
    /// Retries the failed import with an explicit frame rate.
    Q_INVOKABLE void RetryWithFrameRate(const QString &frame_rate);
    /// Returns a display name for a persisted path.
    Q_INVOKABLE QString FileName(const QString &path) const;
    /// Returns whether the window may close without interrupting work.
    Q_INVOKABLE bool RequestClose(void) const noexcept;
    /// Sorts a new event column ascending or toggles the active direction.
    Q_INVOKABLE void ToggleEventSort(int column);

  signals:
    /// Reports a change to the shell document state.
    void documentStateChanged(void);
    /// Reports a change to operation availability.
    void busyChanged(void);
    /// Reports changed source, event-count, or failure presentation.
    void documentPresentationChanged(void);
    /// Reports that the localized status text changed.
    void statusTextChanged(void);
    /// Reports a change to recent-file history.
    void recentFilesChanged(void);
    /// Reports a change to the recent-file preference.
    void rememberRecentFilesChanged(void);
    /// Reports an installed interface-language change.
    void languageChanged(void);
    /// Requests an explicit frame rate for a non-drop-frame EDL.
    void frameRateRequired(void);
    /// Reports a change to the event sorting column or direction.
    void eventSortChanged(void);

  private:
    void HandleDocumentStateChanged(void);
    void StartImport(const QString &path,
                     std::optional<std::string> frame_rate);

    const core::FormatRegistry &registry_;
    QTranslator &translator_;
    presentation::ApplicationLanguage language_;
    presentation::TimelineDocumentViewModel document_view_model_;
    TimelineConfigurationViewModel timeline_configuration_;
    SpreadsheetExportViewModel spreadsheet_export_;
    ApplicationInformationViewModel application_information_;
    SupportBundleViewModel support_bundle_;
    QSortFilterProxyModel event_proxy_model_;
    std::optional<std::string> requested_frame_rate_;
};

} // namespace edit_atlas::frontends::quick

#endif // EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_SHELL_VIEW_MODEL_HPP_
