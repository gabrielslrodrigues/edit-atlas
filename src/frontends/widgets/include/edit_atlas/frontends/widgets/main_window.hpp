#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_MAIN_WINDOW_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_MAIN_WINDOW_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/presentation/translation.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QMainWindow>

#include <filesystem>

class QDragEnterEvent;
class QDropEvent;
class QEvent;
class QTranslator;
class QWidget;

namespace edit_atlas::frontends::widgets {

class ApplicationMenuBar;
class TimelineDocumentController;
class TimelineDocumentView;
class SupportBundleController;

/// Composes the desktop frontend and handles top-level navigation.
class MainWindow final : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(const core::FormatRegistry &registry,
                        QTranslator &translator,
                        presentation::ApplicationLanguage initial_language,
                        std::filesystem::path log_directory,
                        support::DiagnosticEnvironment diagnostic_environment,
                        QWidget *parent = nullptr);
    ~MainWindow(void) override = default;

    MainWindow(const MainWindow &) = delete;
    MainWindow &operator=(const MainWindow &) = delete;
    MainWindow(MainWindow &&) = delete;
    MainWindow &operator=(MainWindow &&) = delete;

  private:
    void changeEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void ChangeLanguage(presentation::ApplicationLanguage language);
    void RetranslateUi(void);
    void SetBusy(bool busy);
    void ShowAboutDialog(void);

    QTranslator &translator_;
    presentation::ApplicationLanguage language_;
    ApplicationMenuBar *application_menu_bar_ = nullptr;
    TimelineDocumentView *timeline_document_view_ = nullptr;
    TimelineDocumentController *timeline_document_controller_ = nullptr;
    SupportBundleController *support_bundle_controller_ = nullptr;
};

} // namespace edit_atlas::frontends::widgets

#endif // EDIT_ATLAS_FRONTENDS_WIDGETS_MAIN_WINDOW_HPP_
