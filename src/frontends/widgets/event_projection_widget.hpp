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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_EVENT_PROJECTION_WIDGET_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_EVENT_PROJECTION_WIDGET_HPP_

#include <span>
#include <vector>

#include "QLabel"
#include "QListWidget"
#include "QPushButton"
#include "QWidget"

#include "edit_atlas/core/timeline_projection.hpp"

namespace edit_atlas::frontends::widgets {

/// Edits an ordered, non-empty timeline event projection.
class EventProjectionWidget final : public QWidget {
  Q_OBJECT

 public:
  explicit EventProjectionWidget(
      std::span<const core::TimelineEventField> projection,
      QWidget* parent = nullptr);
  ~EventProjectionWidget(void) override = default;

  [[nodiscard]] std::vector<core::TimelineEventField> Projection(void) const;

  EventProjectionWidget(const EventProjectionWidget&) = delete;
  EventProjectionWidget& operator=(const EventProjectionWidget&) = delete;
  EventProjectionWidget(EventProjectionWidget&&) = delete;
  EventProjectionWidget& operator=(EventProjectionWidget&&) = delete;

 signals:
  /// Emitted whenever selection or ordering changes.
  void projectionChanged(void);
  void validityChanged(bool valid);

 private:
  void MoveCurrentColumn(int offset);
  void UpdateControls(void);

  QListWidget* columns_ = nullptr;
  QPushButton* move_up_ = nullptr;
  QPushButton* move_down_ = nullptr;
  QLabel* error_ = nullptr;
};

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_EVENT_PROJECTION_WIDGET_HPP_
