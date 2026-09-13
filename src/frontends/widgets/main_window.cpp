// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "edit_atlas/frontends/widgets/main_window.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

#include "QAbstractButton"
#include "QDragEnterEvent"
#include "QDropEvent"
#include "QEvent"
#include "QMessageBox"
#include "QMimeData"
#include "QStatusBar"
#include "QString"
#include "QTranslator"
#include "QUrl"
#include "QWidget"

#include "accessibility.hpp"
#include "edit_atlas/core/format_registry.hpp"
#include "edit_atlas/core/version.hpp"
#include "edit_atlas/frontends/widgets/application_menu_bar.hpp"
#include "edit_atlas/frontends/widgets/support_bundle_controller.hpp"
#include "edit_atlas/frontends/widgets/timeline_document_controller.hpp"
#include "edit_atlas/frontends/widgets/timeline_document_view.hpp"
#include "edit_atlas/presentation/appearance.hpp"
#include "edit_atlas/presentation/translation.hpp"
#include "edit_atlas/support/support_bundle.hpp"

namespace edit_atlas::frontends::widgets {

MainWindow::MainWindow(const core::FormatRegistry& registry,
                       QTranslator& translator,
                       presentation::ApplicationLanguage initial_language,
                       std::filesystem::path log_directory,
                       support::DiagnosticEnvironment diagnostic_environment,
                       presentation::AppearanceController& appearance,
                       QWidget* parent)
    : QMainWindow{parent},
      translator_{translator},
      appearance_{appearance},
      language_{initial_language} {
  SetAutomationIdentifier(*this, u"mainWindow");
  resize(1100, 700);
  setMinimumSize(760, 500);
  setAcceptDrops(true);

  application_menu_bar_ = new ApplicationMenuBar{
      language_, presentation::AppearanceCode(appearance_.Appearance()), this};
  setMenuBar(application_menu_bar_);
  timeline_document_view_ = new TimelineDocumentView{this};
  setCentralWidget(timeline_document_view_);

  timeline_document_controller_ = new TimelineDocumentController{
      registry, *application_menu_bar_, *timeline_document_view_, language_,
      *this};
  support_bundle_controller_ = new SupportBundleController{
      std::move(log_directory), std::move(diagnostic_environment), *this};

  connect(application_menu_bar_,
          &ApplicationMenuBar::exportDiagnosticLogsRequested,
          support_bundle_controller_,
          &SupportBundleController::exportDiagnosticLogs);
  connect(application_menu_bar_, &ApplicationMenuBar::aboutRequested, this,
          &MainWindow::showAboutDialog);
  connect(application_menu_bar_, &ApplicationMenuBar::exitRequested, this,
          &QWidget::close);
  connect(application_menu_bar_, &ApplicationMenuBar::languageSelected, this,
          &MainWindow::changeLanguage);
  connect(application_menu_bar_, &ApplicationMenuBar::appearanceSelected, this,
          [this](const QString& code) {
            appearance_.SetAppearance(presentation::AppearanceFromCode(code));
          });
  connect(&appearance_, &presentation::AppearanceController::appearanceChanged,
          this, [this](void) {
            application_menu_bar_->SetAppearance(
                presentation::AppearanceCode(appearance_.Appearance()));
          });

  connect(timeline_document_controller_,
          &TimelineDocumentController::busyChanged, this, &MainWindow::setBusy);
  connect(support_bundle_controller_, &SupportBundleController::busyChanged,
          this, &MainWindow::setBusy);
  connect(
      timeline_document_controller_,
      &TimelineDocumentController::statusMessageChanged, this,
      [this](const QString& message) { statusBar()->showMessage(message); });
  connect(
      support_bundle_controller_,
      &SupportBundleController::statusMessageChanged, this,
      [this](const QString& message) { statusBar()->showMessage(message); });
  connect(timeline_document_controller_,
          &TimelineDocumentController::statusMessageCleared, statusBar(),
          &QStatusBar::clearMessage);
  connect(support_bundle_controller_,
          &SupportBundleController::statusMessageCleared, statusBar(),
          &QStatusBar::clearMessage);

  RetranslateUi();
}

void MainWindow::changeEvent(QEvent* event) {
  QMainWindow::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    RetranslateUi();
  }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
  if (!timeline_document_controller_->IsBusy() &&
      !support_bundle_controller_->IsBusy() && event->mimeData()->hasUrls() &&
      std::ranges::any_of(event->mimeData()->urls(),
                          [](const QUrl& url) { return url.isLocalFile(); })) {
    event->acceptProposedAction();
  }
}

void MainWindow::dropEvent(QDropEvent* event) {
  const auto urls = event->mimeData()->urls();
  const auto local_file = std::ranges::find_if(
      urls, [](const QUrl& url) { return url.isLocalFile(); });
  if (local_file == urls.end()) {
    return;
  }
  event->acceptProposedAction();
  timeline_document_controller_->OpenTimeline(local_file->toLocalFile());
}

void MainWindow::changeLanguage(presentation::ApplicationLanguage language) {
  if (language == language_) {
    return;
  }

  const auto previous_language = language_;
  language_ = language;
  if (!presentation::SetApplicationLanguage(translator_, language_)) {
    language_ = previous_language;
    static_cast<void>(
        presentation::SetApplicationLanguage(translator_, previous_language));
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

void MainWindow::setBusy(bool busy) {
  application_menu_bar_->SetBusy(busy);
  timeline_document_view_->SetBusy(busy);
  timeline_document_controller_->SetInteractionsEnabled(!busy);
  support_bundle_controller_->SetInteractionsEnabled(!busy);
}

void MainWindow::showAboutDialog(void) {
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
  if (auto* close = dialog.button(QMessageBox::Ok); close != nullptr) {
    SetAutomationIdentifier(*close, u"closeDialogButton");
  }
  dialog.exec();
}

}  // namespace edit_atlas::frontends::widgets
