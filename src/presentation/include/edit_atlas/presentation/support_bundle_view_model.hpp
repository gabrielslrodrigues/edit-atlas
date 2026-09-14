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

#ifndef EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_VIEW_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_VIEW_MODEL_HPP_

#include <expected>
#include <filesystem>
#include <optional>

#include "QObject"

#include "edit_atlas/presentation/support_bundle_workflow.hpp"
#include "edit_atlas/support/support_bundle.hpp"

namespace edit_atlas::presentation {

/// Explains why a support-bundle command was not started.
enum class SupportBundleCommandError {
  /// Another support-bundle export is already running.
  kBusy,
};

/// Result of asking the support-bundle ViewModel to start an export.
using SupportBundleCommandResult =
    std::expected<void, SupportBundleCommandError>;

/// Owns asynchronous diagnostic-bundle export state for desktop frontends.
class SupportBundleViewModel final : public QObject {
  Q_OBJECT

 public:
  /// Creates support-bundle state for the supplied logs and environment.
  SupportBundleViewModel(std::filesystem::path log_directory,
                         support::DiagnosticEnvironment diagnostic_environment,
                         QObject* parent = nullptr);
  /// Destroys the ViewModel after any owned workflow has stopped.
  ~SupportBundleViewModel(void) override = default;

  /// ViewModels are non-copyable QObject owners.
  SupportBundleViewModel(const SupportBundleViewModel&) = delete;
  /// ViewModels are non-copy-assignable QObject owners.
  SupportBundleViewModel& operator=(const SupportBundleViewModel&) = delete;
  /// ViewModels are non-movable QObject owners.
  SupportBundleViewModel(SupportBundleViewModel&&) = delete;
  /// ViewModels are non-move-assignable QObject owners.
  SupportBundleViewModel& operator=(SupportBundleViewModel&&) = delete;

  /// Starts an asynchronous export to the frontend-selected destination.
  [[nodiscard]] SupportBundleCommandResult Export(
      std::filesystem::path destination, bool replace_existing);

  /// Returns whether a support-bundle export is running.
  [[nodiscard]] bool IsBusy(void) const noexcept;
  /// Returns the most recent completed result, or null before completion.
  [[nodiscard]] const support::CreateSupportBundleResult* Result(
      void) const noexcept;

 signals:
  /// Reports a change to `IsBusy()`.
  void busyChanged(void);
  /// Reports that a completed result is available.
  void exportFinished(void);

 private:
  void handleFinished(void);

  std::filesystem::path log_directory_;
  support::DiagnosticEnvironment diagnostic_environment_;
  SupportBundleWorkflow workflow_;
  std::optional<support::CreateSupportBundleResult> result_;
  bool busy_ = false;
};

}  // namespace edit_atlas::presentation

#endif  // EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_VIEW_MODEL_HPP_
