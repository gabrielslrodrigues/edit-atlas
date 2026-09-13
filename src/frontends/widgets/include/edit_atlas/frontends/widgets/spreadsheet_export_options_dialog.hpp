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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_

#include <span>
#include <vector>

#include "QCheckBox"
#include "QComboBox"
#include "QDialog"
#include "QGroupBox"
#include "QLineEdit"
#include "QPushButton"
#include "QString"
#include "QWidget"

#include "edit_atlas/core/editorial_timeline.hpp"
#include "edit_atlas/core/timeline_projection.hpp"
#include "edit_atlas/presentation/translation.hpp"

namespace edit_atlas::frontends::widgets {

class EventProjectionWidget;

/// Collects XLSX presentation options without exposing widget details to the
/// main application window.
class SpreadsheetExportOptionsDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit SpreadsheetExportOptionsDialog(
      presentation::ApplicationLanguage application_language,
      std::span<const core::TimelineEventField> projection,
      QWidget* parent = nullptr);
  ~SpreadsheetExportOptionsDialog(void) override = default;

  [[nodiscard]] std::vector<core::MetadataEntry> Options(void) const;
  [[nodiscard]] std::vector<core::TimelineEventField> EventProjection(
      void) const;
  /// Returns the selected rendered-video path, or an empty string.
  [[nodiscard]] QString VideoPath(void) const;

  SpreadsheetExportOptionsDialog(const SpreadsheetExportOptionsDialog&) =
      delete;
  SpreadsheetExportOptionsDialog& operator=(
      const SpreadsheetExportOptionsDialog&) = delete;
  SpreadsheetExportOptionsDialog(SpreadsheetExportOptionsDialog&&) = delete;
  SpreadsheetExportOptionsDialog& operator=(SpreadsheetExportOptionsDialog&&) =
      delete;

 private:
  void browseForVideo(void);
  void UpdateControls(void);

  QComboBox* workbook_language_ = nullptr;
  QCheckBox* timeline_ = nullptr;
  QCheckBox* diagnostics_ = nullptr;
  EventProjectionWidget* projection_ = nullptr;
  QGroupBox* video_group_ = nullptr;
  QLineEdit* video_path_ = nullptr;
  QPushButton* continue_ = nullptr;
};

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_SPREADSHEET_EXPORT_OPTIONS_DIALOG_HPP_
