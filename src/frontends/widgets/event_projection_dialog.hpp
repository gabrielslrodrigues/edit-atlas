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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_EVENT_PROJECTION_DIALOG_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_EVENT_PROJECTION_DIALOG_HPP_

#include <span>
#include <vector>

#include "QDialog"
#include "QPushButton"
#include "QWidget"

#include "edit_atlas/core/timeline_projection.hpp"

namespace edit_atlas::frontends::widgets {

class EventProjectionWidget;

/// Collects a valid event-column projection for templates and exports.
class EventProjectionDialog final : public QDialog {
  Q_OBJECT

 public:
  explicit EventProjectionDialog(
      std::span<const core::TimelineEventField> projection,
      QWidget* parent = nullptr);
  ~EventProjectionDialog(void) override = default;

  [[nodiscard]] std::vector<core::TimelineEventField> Projection(void) const;

  EventProjectionDialog(const EventProjectionDialog&) = delete;
  EventProjectionDialog& operator=(const EventProjectionDialog&) = delete;
  EventProjectionDialog(EventProjectionDialog&&) = delete;
  EventProjectionDialog& operator=(EventProjectionDialog&&) = delete;

 private:
  EventProjectionWidget* projection_ = nullptr;
  QPushButton* save_ = nullptr;
};

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_EVENT_PROJECTION_DIALOG_HPP_
