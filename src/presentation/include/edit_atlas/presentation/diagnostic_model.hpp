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

#ifndef EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_MODEL_HPP_

#include <span>
#include <vector>

#include "QAbstractTableModel"
#include "QModelIndex"
#include "QObject"
#include "QVariant"
#include "Qt"

#include "edit_atlas/core/editorial_timeline.hpp"

namespace edit_atlas::presentation {

/// Presents localized diagnostics as a frontend-neutral Qt table model.
class DiagnosticModel final : public QAbstractTableModel {
  Q_OBJECT

 public:
  /// Role containing language-independent values suitable for sorting.
  static constexpr int kSortRole = Qt::UserRole;

  /// Creates an empty diagnostic model with an optional QObject parent.
  explicit DiagnosticModel(QObject* parent = nullptr);
  /// Destroys the model and its copied diagnostics.
  ~DiagnosticModel(void) override = default;

  /// Diagnostic models are non-copyable QObject owners.
  DiagnosticModel(const DiagnosticModel&) = delete;
  /// Diagnostic models are non-copy-assignable QObject owners.
  DiagnosticModel& operator=(const DiagnosticModel&) = delete;
  /// Diagnostic models are non-movable QObject owners.
  DiagnosticModel(DiagnosticModel&&) = delete;
  /// Diagnostic models are non-move-assignable QObject owners.
  DiagnosticModel& operator=(DiagnosticModel&&) = delete;

  /// Returns the number of diagnostics for a root model index.
  [[nodiscard]] int rowCount(
      const QModelIndex& parent = QModelIndex{}) const override;
  /// Returns the severity, line, and message column count.
  [[nodiscard]] int columnCount(
      const QModelIndex& parent = QModelIndex{}) const override;
  /// Returns localized display data, stable sort data, or diagnostic codes.
  [[nodiscard]] QVariant data(const QModelIndex& index,
                              int role = Qt::DisplayRole) const override;
  /// Returns localized horizontal column labels.
  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role = Qt::DisplayRole) const override;

  /// Replaces the diagnostics presented by the model.
  void SetDiagnostics(std::span<const core::Diagnostic> diagnostics);
  /// Notifies views that localized headers and display values changed.
  void Retranslate(void);

 private:
  std::vector<core::Diagnostic> diagnostics_;
};

}  // namespace edit_atlas::presentation

#endif  // EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_MODEL_HPP_
