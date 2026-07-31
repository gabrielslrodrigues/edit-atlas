#include <edit_atlas/app/main_window.hpp>

#include <edit_atlas/app/application_menu_bar.hpp>
#include <edit_atlas/app/document_controller.hpp>
#include <edit_atlas/app/document_view.hpp>
#include <edit_atlas/app/support_bundle_controller.hpp>

#include <edit_atlas/core/version.hpp>

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QStatusBar>
#include <QString>
#include <QTranslator>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

namespace edit_atlas::app {

MainWindow::MainWindow(const core::FormatRegistry &registry,
                       QTranslator &translator,
                       ApplicationLanguage initial_language,
                       std::filesystem::path log_directory,
                       support::DiagnosticEnvironment diagnostic_environment,
                       QWidget *parent)
    : QMainWindow{parent}, translator_{translator},
      language_{initial_language} {
    setObjectName(QStringLiteral("mainWindow"));
    resize(1100, 700);
    setMinimumSize(760, 500);
    setAcceptDrops(true);

    application_menu_bar_ = new ApplicationMenuBar{language_, this};
    setMenuBar(application_menu_bar_);
    document_view_ = new DocumentView{this};
    setCentralWidget(document_view_);

    document_controller_ = new DocumentController{
        registry, *application_menu_bar_, *document_view_, language_, *this};
    support_bundle_controller_ = new SupportBundleController{
        std::move(log_directory), std::move(diagnostic_environment), *this};

    connect(application_menu_bar_,
            &ApplicationMenuBar::ExportDiagnosticLogsRequested,
            support_bundle_controller_,
            &SupportBundleController::ExportDiagnosticLogs);
    connect(application_menu_bar_, &ApplicationMenuBar::AboutRequested, this,
            &MainWindow::ShowAboutDialog);
    connect(application_menu_bar_, &ApplicationMenuBar::ExitRequested, this,
            &QWidget::close);
    connect(application_menu_bar_, &ApplicationMenuBar::LanguageSelected, this,
            &MainWindow::ChangeLanguage);

    connect(document_controller_, &DocumentController::BusyChanged, this,
            &MainWindow::SetBusy);
    connect(support_bundle_controller_, &SupportBundleController::BusyChanged,
            this, &MainWindow::SetBusy);
    connect(
        document_controller_, &DocumentController::StatusMessageChanged, this,
        [this](const QString &message) { statusBar()->showMessage(message); });
    connect(
        support_bundle_controller_,
        &SupportBundleController::StatusMessageChanged, this,
        [this](const QString &message) { statusBar()->showMessage(message); });
    connect(document_controller_, &DocumentController::StatusMessageCleared,
            statusBar(), &QStatusBar::clearMessage);
    connect(support_bundle_controller_,
            &SupportBundleController::StatusMessageCleared, statusBar(),
            &QStatusBar::clearMessage);

    RetranslateUi();
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUi();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (!document_controller_->IsBusy() &&
        !support_bundle_controller_->IsBusy() && event->mimeData()->hasUrls() &&
        std::ranges::any_of(event->mimeData()->urls(), [](const QUrl &url) {
            return url.isLocalFile();
        })) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    const auto urls = event->mimeData()->urls();
    const auto local_file = std::ranges::find_if(
        urls, [](const QUrl &url) { return url.isLocalFile(); });
    if (local_file == urls.end()) {
        return;
    }
    event->acceptProposedAction();
    document_controller_->OpenDocument(local_file->toLocalFile());
}

void MainWindow::ChangeLanguage(ApplicationLanguage language) {
    if (language == language_) {
        return;
    }

    const auto previous_language = language_;
    language_ = language;
    if (!SetApplicationLanguage(translator_, language_)) {
        language_ = previous_language;
        static_cast<void>(
            SetApplicationLanguage(translator_, previous_language));
        RetranslateUi();
        return;
    }

    SaveApplicationLanguage(language_);
    RetranslateUi();
}

void MainWindow::RetranslateUi(void) {
    setWindowTitle(tr("Edit Atlas"));
    application_menu_bar_->SetLanguage(language_);
    application_menu_bar_->RetranslateUi();
    document_view_->RetranslateUi();
    document_controller_->SetLanguage(language_);
    support_bundle_controller_->RetranslateUi();
}

void MainWindow::SetBusy(bool busy) {
    application_menu_bar_->SetBusy(busy);
    document_view_->SetBusy(busy);
    document_controller_->SetInteractionsEnabled(!busy);
    support_bundle_controller_->SetInteractionsEnabled(!busy);
}

void MainWindow::ShowAboutDialog(void) {
    const auto version = QString::fromStdString(std::string{core::Version()});
    QMessageBox::about(
        this, tr("About Edit Atlas"),
        tr("Edit Atlas %1\n\nInspect editorial timelines and export "
           "structured reports.")
            .arg(version));
}

} // namespace edit_atlas::app
