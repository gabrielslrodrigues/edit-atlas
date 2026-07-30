#ifndef EDIT_ATLAS_APP_MAIN_WINDOW_HPP_
#define EDIT_ATLAS_APP_MAIN_WINDOW_HPP_

#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/document_export_service.hpp>
#include <edit_atlas/services/document_import_service.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QFutureWatcher>
#include <QMainWindow>
#include <QString>

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

class QAction;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QGroupBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QTreeWidget;
class QTranslator;
class QWidget;

namespace edit_atlas::app {

class TimelineEventModel;

/// The top-level desktop shell shared by all document workflows.
class MainWindow final : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(const core::FormatRegistry &registry,
                        QTranslator &translator,
                        ApplicationLanguage initial_language,
                        std::filesystem::path log_directory,
                        support::DiagnosticEnvironment diagnostic_environment,
                        QWidget *parent = nullptr);
    ~MainWindow(void) override;

    MainWindow(const MainWindow &) = delete;
    MainWindow &operator=(const MainWindow &) = delete;
    MainWindow(MainWindow &&) = delete;
    MainWindow &operator=(MainWindow &&) = delete;

  private:
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void BuildMenus(void);
    void BuildUi(void);
    void ChangeLanguage(ApplicationLanguage language);
    void ClearDocument(void);
    [[nodiscard]] QString
    DiagnosticMessage(const core::Diagnostic &diagnostic) const;
    [[nodiscard]] QString
    DiagnosticSummary(const std::vector<core::Diagnostic> &diagnostics) const;
    void ExportSpreadsheet(void);
    void ExportDiagnosticLogs(void);
    void HandleExportFinished(void);
    void HandleImportFinished(void);
    void HandleSupportBundleFinished(void);
    void OpenDocument(void);
    void OpenDocument(const QString &path);
    void PopulateDiagnostics(const std::vector<core::Diagnostic> &diagnostics);
    void PopulateTimeline(void);
    void RememberRecentFile(const QString &path);
    void RetranslateUi(void);
    void RetranslateTimeline(void);
    void SetRememberRecentFiles(bool enabled);
    void ShowExportFailure(const services::DocumentExportFailure &failure);
    void ShowImportFailure(const services::DocumentImportFailure &failure);
    void ShowSupportBundleFailure(const support::SupportBundleFailure &failure);
    void ShowAboutDialog(void);
    void StartImport(const QString &path,
                     std::optional<std::string> frame_rate = std::nullopt);
    void UpdateRecentFilesMenu(void);

    const core::FormatRegistry &registry_;
    services::DocumentExportService document_export_service_;
    services::DocumentImportService document_import_service_;
    QTranslator &translator_;
    ApplicationLanguage language_;
    std::filesystem::path log_directory_;
    support::DiagnosticEnvironment diagnostic_environment_;
    std::optional<core::TimelineDocument> document_;
    QString current_path_;
    std::optional<std::string> requested_frame_rate_;
    std::optional<services::DocumentImportFailureKind> last_import_error_;
    std::error_code last_filesystem_error_;
    std::vector<core::Diagnostic> last_diagnostics_;
    QFutureWatcher<services::ExportDocumentResult> export_watcher_;
    QFutureWatcher<services::ImportDocumentResult> import_watcher_;
    QFutureWatcher<support::CreateSupportBundleResult> support_bundle_watcher_;
    QAction *open_action_ = nullptr;
    QAction *export_action_ = nullptr;
    QAction *remember_recent_action_ = nullptr;
    QAction *exit_action_ = nullptr;
    QAction *export_logs_action_ = nullptr;
    QAction *about_action_ = nullptr;
    QMenu *file_menu_ = nullptr;
    QMenu *recent_files_menu_ = nullptr;
    QMenu *help_menu_ = nullptr;
    QComboBox *language_selector_ = nullptr;
    QLabel *title_label_ = nullptr;
    QLabel *subtitle_label_ = nullptr;
    QStackedWidget *document_stack_ = nullptr;
    QLabel *empty_title_label_ = nullptr;
    QLabel *empty_description_label_ = nullptr;
    QPushButton *empty_open_button_ = nullptr;
    QLabel *loading_label_ = nullptr;
    QLabel *failure_title_label_ = nullptr;
    QLabel *failure_description_label_ = nullptr;
    QPushButton *failure_open_button_ = nullptr;
    QLabel *timeline_title_label_ = nullptr;
    QPushButton *timeline_export_button_ = nullptr;
    QLabel *timeline_summary_label_ = nullptr;
    QLineEdit *event_filter_ = nullptr;
    QTableView *event_table_ = nullptr;
    TimelineEventModel *event_model_ = nullptr;
    QSortFilterProxyModel *event_proxy_model_ = nullptr;
    QGroupBox *diagnostics_group_ = nullptr;
    QTreeWidget *diagnostics_tree_ = nullptr;
    QLabel *privacy_label_ = nullptr;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_MAIN_WINDOW_HPP_
