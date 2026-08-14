#include <edit_atlas/app/main_window.hpp>

#include "accessibility.hpp"

#include <edit_atlas/app/application_menu_bar.hpp>
#include <edit_atlas/app/support_bundle_controller.hpp>
#include <edit_atlas/app/timeline_document_controller.hpp>
#include <edit_atlas/app/timeline_document_view.hpp>

#include <edit_atlas/core/version.hpp>

#include <QAbstractButton>
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
                       presentation::ApplicationLanguage initial_language,
                       std::filesystem::path log_directory,
                       support::DiagnosticEnvironment diagnostic_environment,
                       QWidget *parent)
    : QMainWindow{parent}, translator_{translator},
      language_{initial_language} {
    SetAutomationIdentifier(*this, u"mainWindow");
    resize(1100, 700);
    setMinimumSize(760, 500);
    setAcceptDrops(true);

    application_menu_bar_ = new ApplicationMenuBar{language_, this};
    setMenuBar(application_menu_bar_);
    timeline_document_view_ = new TimelineDocumentView{this};
    setCentralWidget(timeline_document_view_);

    timeline_document_controller_ = new TimelineDocumentController{
        registry, *application_menu_bar_, *timeline_document_view_, language_,
        *this};
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

    connect(timeline_document_controller_,
            &TimelineDocumentController::BusyChanged, this,
            &MainWindow::SetBusy);
    connect(support_bundle_controller_, &SupportBundleController::BusyChanged,
            this, &MainWindow::SetBusy);
    connect(
        timeline_document_controller_,
        &TimelineDocumentController::StatusMessageChanged, this,
        [this](const QString &message) { statusBar()->showMessage(message); });
    connect(
        support_bundle_controller_,
        &SupportBundleController::StatusMessageChanged, this,
        [this](const QString &message) { statusBar()->showMessage(message); });
    connect(timeline_document_controller_,
            &TimelineDocumentController::StatusMessageCleared, statusBar(),
            &QStatusBar::clearMessage);
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
    if (!timeline_document_controller_->IsBusy() &&
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
    timeline_document_controller_->OpenTimeline(local_file->toLocalFile());
}

void MainWindow::ChangeLanguage(presentation::ApplicationLanguage language) {
    if (language == language_) {
        return;
    }

    const auto previous_language = language_;
    language_ = language;
    if (!presentation::SetApplicationLanguage(translator_, language_)) {
        language_ = previous_language;
        static_cast<void>(presentation::SetApplicationLanguage(
            translator_, previous_language));
        RetranslateUi();
        return;
    }

    presentation::SaveApplicationLanguage(language_);
    RetranslateUi();
}

void MainWindow::RetranslateUi(void) {
    setWindowTitle(tr("Edit Atlas"));
    application_menu_bar_->SetLanguage(language_);
    application_menu_bar_->RetranslateUi();
    timeline_document_view_->RetranslateUi();
    timeline_document_controller_->SetLanguage(language_);
    support_bundle_controller_->RetranslateUi();
}

void MainWindow::SetBusy(bool busy) {
    application_menu_bar_->SetBusy(busy);
    timeline_document_view_->SetBusy(busy);
    timeline_document_controller_->SetInteractionsEnabled(!busy);
    support_bundle_controller_->SetInteractionsEnabled(!busy);
}

void MainWindow::ShowAboutDialog(void) {
    const auto version = QString::fromStdString(std::string{core::Version()});
    QMessageBox dialog{
        QMessageBox::Information,
        tr("About Edit Atlas"),
        tr("Edit Atlas %1\n\nInspect editorial timelines and export "
           "structured reports.")
            .arg(version),
        QMessageBox::Ok,
        this,
    };
    SetAutomationIdentifier(dialog, u"aboutDialog");
    if (auto *close = dialog.button(QMessageBox::Ok); close != nullptr) {
        SetAutomationIdentifier(*close, u"closeDialogButton");
    }
    dialog.exec();
}

} // namespace edit_atlas::app
