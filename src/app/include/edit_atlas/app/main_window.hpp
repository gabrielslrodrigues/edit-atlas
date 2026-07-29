#ifndef EDIT_ATLAS_APP_MAIN_WINDOW_HPP_
#define EDIT_ATLAS_APP_MAIN_WINDOW_HPP_

#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/format_registry.hpp>

#include <QMainWindow>
#include <QObject>

class QAction;
class QComboBox;
class QEvent;
class QLabel;
class QMenu;
class QTranslator;
class QWidget;

namespace edit_atlas::app {

/// The top-level desktop shell shared by all document workflows.
class MainWindow final : public QMainWindow {
    Q_OBJECT

  public:
    explicit MainWindow(const core::FormatRegistry &registry,
                        QTranslator &translator,
                        ApplicationLanguage initial_language,
                        QWidget *parent = nullptr);
    ~MainWindow(void) override = default;

    MainWindow(const MainWindow &) = delete;
    MainWindow &operator=(const MainWindow &) = delete;
    MainWindow(MainWindow &&) = delete;
    MainWindow &operator=(MainWindow &&) = delete;

  private:
    void changeEvent(QEvent *event) override;

    void BuildMenus(void);
    void BuildUi(void);
    void ChangeLanguage(ApplicationLanguage language);
    void RetranslateUi(void);
    void ShowAboutDialog(void);

    const core::FormatRegistry &registry_;
    QTranslator &translator_;
    ApplicationLanguage language_;
    QAction *exit_action_ = nullptr;
    QAction *about_action_ = nullptr;
    QMenu *file_menu_ = nullptr;
    QMenu *help_menu_ = nullptr;
    QComboBox *language_selector_ = nullptr;
    QLabel *title_label_ = nullptr;
    QLabel *subtitle_label_ = nullptr;
    QLabel *empty_title_label_ = nullptr;
    QLabel *empty_description_label_ = nullptr;
    QLabel *privacy_label_ = nullptr;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_MAIN_WINDOW_HPP_
