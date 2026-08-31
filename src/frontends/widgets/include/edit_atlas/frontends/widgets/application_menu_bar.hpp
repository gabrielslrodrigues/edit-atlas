#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_MENU_BAR_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_MENU_BAR_HPP_

#include <edit_atlas/presentation/translation.hpp>

#include <QMenuBar>
#include <QString>

class QAction;
class QMenu;
class QWidget;

namespace edit_atlas::frontends::widgets {

/// Owns application actions, language selection, and recent-file settings.
class ApplicationMenuBar final : public QMenuBar {
    Q_OBJECT

  public:
    explicit ApplicationMenuBar(
        presentation::ApplicationLanguage initial_language,
        QWidget *parent = nullptr);
    ~ApplicationMenuBar(void) override = default;

    ApplicationMenuBar(const ApplicationMenuBar &) = delete;
    ApplicationMenuBar &operator=(const ApplicationMenuBar &) = delete;
    ApplicationMenuBar(ApplicationMenuBar &&) = delete;
    ApplicationMenuBar &operator=(ApplicationMenuBar &&) = delete;

    void RememberRecentFile(const QString &path);
    void RetranslateUi(void);
    void SetBusy(bool busy);
    void SetDocumentAvailable(bool available);
    void SetExportAvailable(bool available);
    void SetLanguage(presentation::ApplicationLanguage language);

  signals:
    void AboutRequested(void);
    void ExitRequested(void);
    void ExportDiagnosticLogsRequested(void);
    void ExportSpreadsheetRequested(void);
    void LanguageSelected(presentation::ApplicationLanguage language);
    void OpenPathRequested(const QString &path);
    void OpenRequested(void);

  private:
    void BuildUi(presentation::ApplicationLanguage initial_language);
    void SetRememberRecentFiles(bool enabled);
    void UpdateActions(void);
    void UpdateRecentFilesMenu(void);

    bool busy_ = false;
    bool document_available_ = false;
    bool export_available_ = true;
    QMenu *file_menu_ = nullptr;
    QAction *open_action_ = nullptr;
    QMenu *recent_files_menu_ = nullptr;
    QAction *remember_recent_action_ = nullptr;
    QAction *export_action_ = nullptr;
    QAction *exit_action_ = nullptr;
    QMenu *help_menu_ = nullptr;
    QAction *export_logs_action_ = nullptr;
    QAction *about_action_ = nullptr;
    QMenu *language_menu_ = nullptr;
    QAction *brazilian_portuguese_action_ = nullptr;
    QAction *english_action_ = nullptr;
};

} // namespace edit_atlas::frontends::widgets

#endif // EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_MENU_BAR_HPP_
