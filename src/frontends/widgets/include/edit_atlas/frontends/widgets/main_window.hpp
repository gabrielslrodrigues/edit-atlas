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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_MAIN_WINDOW_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_MAIN_WINDOW_HPP_

#include <filesystem>

#include "QDragEnterEvent"
#include "QDropEvent"
#include "QEvent"
#include "QMainWindow"
#include "QTranslator"
#include "QWidget"

#include "edit_atlas/core/format_registry.hpp"
#include "edit_atlas/presentation/appearance.hpp"
#include "edit_atlas/presentation/translation.hpp"
#include "edit_atlas/support/support_bundle.hpp"

namespace edit_atlas::frontends::widgets {

class ApplicationMenuBar;
class TimelineDocumentController;
class TimelineDocumentView;
class SupportBundleController;

/// Composes the desktop frontend and handles top-level navigation.
class MainWindow final : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(const core::FormatRegistry& registry,
                      QTranslator& translator,
                      presentation::ApplicationLanguage initial_language,
                      std::filesystem::path log_directory,
                      support::DiagnosticEnvironment diagnostic_environment,
                      presentation::AppearanceController& appearance,
                      QWidget* parent = nullptr);
  ~MainWindow(void) override = default;

  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;
  MainWindow(MainWindow&&) = delete;
  MainWindow& operator=(MainWindow&&) = delete;

 private:
  void changeEvent(QEvent* event) override;
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  void changeLanguage(presentation::ApplicationLanguage language);
  void RetranslateUi(void);
  void setBusy(bool busy);
  void showAboutDialog(void);

  QTranslator& translator_;
  presentation::AppearanceController& appearance_;
  presentation::ApplicationLanguage language_;
  ApplicationMenuBar* application_menu_bar_ = nullptr;
  TimelineDocumentView* timeline_document_view_ = nullptr;
  TimelineDocumentController* timeline_document_controller_ = nullptr;
  SupportBundleController* support_bundle_controller_ = nullptr;
};

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_MAIN_WINDOW_HPP_
