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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_DOCUMENT_CONTROLLER_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_DOCUMENT_CONTROLLER_HPP_

#include <optional>
#include <string>

#include "QObject"
#include "QProgressDialog"
#include "QString"
#include "QWidget"

#include "edit_atlas/core/format_registry.hpp"
#include "edit_atlas/presentation/timeline_document_view_model.hpp"
#include "edit_atlas/presentation/timeline_filter_model.hpp"
#include "edit_atlas/presentation/translation.hpp"
#include "edit_atlas/services/timeline_document_export_service.hpp"
#include "edit_atlas/services/timeline_rendered_video_export_service.hpp"

namespace edit_atlas::frontends::widgets {

class ApplicationMenuBar;
class TimelineDocumentView;
class TimelineTemplateController;

/// Adapts timeline-document ViewModel commands to Qt Widgets interactions.
class TimelineDocumentController final : public QObject {
  Q_OBJECT

 public:
  TimelineDocumentController(const core::FormatRegistry& registry,
                             ApplicationMenuBar& menu_bar,
                             TimelineDocumentView& view,
                             presentation::ApplicationLanguage language,
                             QWidget& window);
  ~TimelineDocumentController(void) override = default;

  TimelineDocumentController(const TimelineDocumentController&) = delete;
  TimelineDocumentController& operator=(const TimelineDocumentController&) =
      delete;
  TimelineDocumentController(TimelineDocumentController&&) = delete;
  TimelineDocumentController& operator=(TimelineDocumentController&&) = delete;

  void exportSpreadsheet(void);
  [[nodiscard]] bool IsBusy(void) const noexcept;
  /// Prompts the user to choose a file, then opens it.
  void OpenTimeline(void);
  /// Opens an already-known local path directly, without prompting.
  void OpenTimeline(const QString& path);
  void SetInteractionsEnabled(bool enabled);
  void SetLanguage(presentation::ApplicationLanguage language);

 signals:
  void busyChanged(bool busy);
  void statusMessageChanged(const QString& message);
  void statusMessageCleared(void);

 private:
  void applyFilter(void);
  void ClearTimeline(void);
  /// Prompts to replace an existing destination file; returns true when the
  /// destination doesn't exist yet or the user confirmed replacing it.
  bool ConfirmSpreadsheetOverwrite(const QString& destination);
  void handleDocumentStateChanged(void);
  void handleExportFinished(void);
  void HandleFilterChanged(void);
  void ShowExportFailure(
      const services::TimelineDocumentExportFailure& failure);
  void ShowRenderedVideoExportFailure(
      const services::TimelineRenderedVideoExportFailure& failure);
  /// Creates and shows the export_progress_ dialog for a video-backed export.
  void ShowSpreadsheetExportProgress(void);
  void ShowSpreadsheetExporterUnavailable(void);
  void StartImport(const QString& path,
                   std::optional<std::string> frame_rate = std::nullopt);

  const core::FormatRegistry& registry_;
  ApplicationMenuBar& menu_bar_;
  TimelineDocumentView& view_;
  presentation::ApplicationLanguage language_;
  QWidget& window_;
  presentation::TimelineDocumentViewModel view_model_;
  presentation::TimelineFilterModel filter_model_;
  TimelineTemplateController* template_controller_ = nullptr;
  QProgressDialog* export_progress_ = nullptr;
  bool interactions_enabled_ = true;
  std::optional<std::string> requested_frame_rate_;
};

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_DOCUMENT_CONTROLLER_HPP_
