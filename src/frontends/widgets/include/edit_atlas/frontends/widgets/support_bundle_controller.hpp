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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_SUPPORT_BUNDLE_CONTROLLER_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_SUPPORT_BUNDLE_CONTROLLER_HPP_

#include <filesystem>

#include "QObject"
#include "QString"
#include "QWidget"

#include "edit_atlas/presentation/support_bundle_view_model.hpp"
#include "edit_atlas/support/support_bundle.hpp"

namespace edit_atlas::frontends::widgets {

/// Adapts support-bundle ViewModel commands to Qt Widgets interactions.
class SupportBundleController final : public QObject {
  Q_OBJECT

 public:
  SupportBundleController(std::filesystem::path log_directory,
                          support::DiagnosticEnvironment diagnostic_environment,
                          QWidget& window);
  ~SupportBundleController(void) override = default;

  SupportBundleController(const SupportBundleController&) = delete;
  SupportBundleController& operator=(const SupportBundleController&) = delete;
  SupportBundleController(SupportBundleController&&) = delete;
  SupportBundleController& operator=(SupportBundleController&&) = delete;

  void exportDiagnosticLogs(void);
  [[nodiscard]] bool IsBusy(void) const noexcept;
  void RetranslateUi(void);
  void SetInteractionsEnabled(bool enabled);

 signals:
  void busyChanged(bool busy);
  void statusMessageChanged(const QString& message);
  void statusMessageCleared(void);

 private:
  void handleBusyChanged(void);
  void handleFinished(void);
  void ShowFailure(const support::SupportBundleFailure& failure);

  QWidget& window_;
  presentation::SupportBundleViewModel view_model_;
  bool interactions_enabled_ = true;
};

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_SUPPORT_BUNDLE_CONTROLLER_HPP_
