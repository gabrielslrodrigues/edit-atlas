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

#ifndef EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_MODEL_HPP_

#include <cstddef>
#include <vector>

#include "QAbstractTableModel"
#include "QModelIndex"
#include "QObject"
#include "QString"
#include "QVariant"
#include "Qt"

#include "edit_atlas/core/editorial_timeline.hpp"

namespace edit_atlas::presentation {

/// Presents timeline events lazily for sorting and filtering in the desktop UI.
class TimelineEventModel final : public QAbstractTableModel {
  Q_OBJECT

 public:
  /// Role containing language-independent values suitable for sorting.
  static constexpr int kSortRole = Qt::UserRole;

  /// Creates an empty model with an optional QObject parent.
  explicit TimelineEventModel(QObject* parent = nullptr);
  /// Destroys the non-owning model.
  ~TimelineEventModel(void) override = default;

  /// Timeline event models are non-copyable QObject owners.
  TimelineEventModel(const TimelineEventModel&) = delete;
  /// Timeline event models are non-copy-assignable QObject owners.
  TimelineEventModel& operator=(const TimelineEventModel&) = delete;
  /// Timeline event models are non-movable QObject owners.
  TimelineEventModel(TimelineEventModel&&) = delete;
  /// Timeline event models are non-move-assignable QObject owners.
  TimelineEventModel& operator=(TimelineEventModel&&) = delete;

  /// Returns the number of selected events for a root model index.
  [[nodiscard]] int rowCount(
      const QModelIndex& parent = QModelIndex{}) const override;
  /// Returns the fixed number of event fields exposed by the model.
  [[nodiscard]] int columnCount(
      const QModelIndex& parent = QModelIndex{}) const override;
  /// Returns localized display or sorting data for one event field.
  [[nodiscard]] QVariant data(const QModelIndex& index,
                              int role = Qt::DisplayRole) const override;
  /// Returns localized horizontal field labels and vertical row labels.
  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role = Qt::DisplayRole) const override;

  /// Changes the non-owning document displayed by the model.
  void SetDocument(const core::TimelineDocument* document);
  /// Selects source-document event indices to display in the supplied order.
  void SetEventSelection(std::vector<std::size_t> event_indices);
  /// Notifies views that localized headers and display values changed.
  void Retranslate(void);

 private:
  [[nodiscard]] QString EditTypeText(core::EditType edit_type) const;
  [[nodiscard]] QString TrackText(const core::EditEvent& event) const;

  const core::TimelineDocument* document_ = nullptr;
  std::vector<std::size_t> event_indices_;
};

}  // namespace edit_atlas::presentation

#endif  // EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_MODEL_HPP_
