#ifndef EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_SHELL_VIEW_MODEL_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_SHELL_VIEW_MODEL_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/presentation/timeline_document_view_model.hpp>
#include <edit_atlas/presentation/translation.hpp>

#include <QObject>
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
                   DocumentStateChanged)
    Q_PROPERTY(bool busy READ IsBusy NOTIFY BusyChanged)
    Q_PROPERTY(QString sourceFileName READ SourceFileName NOTIFY
                   DocumentPresentationChanged)
    Q_PROPERTY(qulonglong eventCount READ EventCount NOTIFY
                   DocumentPresentationChanged)
    Q_PROPERTY(QString statusText READ StatusText NOTIFY StatusTextChanged)
    Q_PROPERTY(
        QString errorText READ ErrorText NOTIFY DocumentPresentationChanged)
    Q_PROPERTY(QStringList importFilePatterns READ ImportFilePatterns CONSTANT)
    Q_PROPERTY(
        QStringList recentFiles READ RecentFiles NOTIFY RecentFilesChanged)
    Q_PROPERTY(bool rememberRecentFiles READ RememberRecentFiles WRITE
                   SetRememberRecentFiles NOTIFY RememberRecentFilesChanged)
    Q_PROPERTY(QString languageCode READ LanguageCode WRITE SetLanguageCode
                   NOTIFY LanguageChanged)

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
    /// Returns whether a document operation prevents new interaction.
    [[nodiscard]] bool IsBusy(void) const noexcept;
    /// Returns the active source file name without its parent directory.
    [[nodiscard]] QString SourceFileName(void) const;
    /// Returns the number of imported timeline events.
    [[nodiscard]] qulonglong EventCount(void) const noexcept;
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

  signals:
    /// Reports a change to the shell document state.
    void DocumentStateChanged(void);
    /// Reports a change to operation availability.
    void BusyChanged(void);
    /// Reports changed source, event-count, or failure presentation.
    void DocumentPresentationChanged(void);
    /// Reports that the localized status text changed.
    void StatusTextChanged(void);
    /// Reports a change to recent-file history.
    void RecentFilesChanged(void);
    /// Reports a change to the recent-file preference.
    void RememberRecentFilesChanged(void);
    /// Reports an installed interface-language change.
    void LanguageChanged(void);
    /// Requests an explicit frame rate for a non-drop-frame EDL.
    void frameRateRequired(void);

  private:
    void HandleDocumentStateChanged(void);
    void StartImport(const QString &path,
                     std::optional<std::string> frame_rate);

    const core::FormatRegistry &registry_;
    QTranslator &translator_;
    presentation::ApplicationLanguage language_;
    presentation::TimelineDocumentViewModel document_view_model_;
    std::optional<std::string> requested_frame_rate_;
};

} // namespace edit_atlas::frontends::quick

#endif // EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_SHELL_VIEW_MODEL_HPP_
