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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_TEMPLATE_CONTROLLER_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_TEMPLATE_CONTROLLER_HPP_

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "QObject"
#include "QString"
#include "QWidget"

#include "edit_atlas/core/timeline_projection.hpp"
#include "edit_atlas/presentation/timeline_template_view_model.hpp"
#include "edit_atlas/services/timeline_filter.hpp"
#include "edit_atlas/services/timeline_template_service.hpp"

namespace edit_atlas::frontends::widgets {

class TimelineDocumentView;

/// Adapts timeline-template ViewModel commands to Qt Widgets interactions.
class TimelineTemplateController final : public QObject {
  Q_OBJECT

 public:
  TimelineTemplateController(TimelineDocumentView& view, QWidget& window,
                             QObject* parent = nullptr);
  ~TimelineTemplateController(void) override = default;

  TimelineTemplateController(const TimelineTemplateController&) = delete;
  TimelineTemplateController& operator=(const TimelineTemplateController&) =
      delete;
  TimelineTemplateController(TimelineTemplateController&&) = delete;
  TimelineTemplateController& operator=(TimelineTemplateController&&) = delete;

  /// Returns the current ordered export projection.
  [[nodiscard]] std::span<const core::TimelineEventField> EventProjection(
      void) const noexcept;

  /// Restores the active template, or fresh defaults when none is active.
  void RestoreForTimeline(void);

  /// Replaces the current export projection and refreshes the View.
  void SetEventProjection(
      std::vector<core::TimelineEventField> event_projection);

  /// Synchronizes the current filter and whether it can be persisted.
  void SetFilterState(services::TimelineFilterQuery filter, bool valid);

 private:
  void applyTemplate(const QString& identifier);
  void deleteTemplate(void);
  void duplicateTemplate(void);
  void editExportColumns(void);
  void LoadTemplates(void);
  [[nodiscard]] std::optional<std::string> PromptForTemplateName(
      const QString& title, const QString& label, const QString& initial);
  void RefreshTemplateState(void);
  void renameTemplate(void);
  void saveTemplate(void);
  void ShowCommandFailure(
      const QString& title,
      const presentation::TimelineTemplateCommandFailure& failure);
  void ShowInvalidFilter(const QString& title, const QString& description);
  void ShowServiceFailure(const QString& title,
                          const services::TimelineTemplateFailure& failure);
  void updateTemplate(void);

  TimelineDocumentView& view_;
  QWidget& window_;
  presentation::TimelineTemplateViewModel view_model_;
};

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_TEMPLATE_CONTROLLER_HPP_
