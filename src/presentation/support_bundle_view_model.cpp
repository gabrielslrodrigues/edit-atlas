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

#include "edit_atlas/presentation/support_bundle_view_model.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <utility>

#include "QObject"
#include "spdlog/spdlog.h"

#include "edit_atlas/presentation/support_bundle_workflow.hpp"
#include "edit_atlas/support/support_bundle.hpp"

namespace edit_atlas::presentation {

SupportBundleViewModel::SupportBundleViewModel(
    std::filesystem::path log_directory,
    support::DiagnosticEnvironment diagnostic_environment, QObject* parent)
    : QObject{parent},
      log_directory_{std::move(log_directory)},
      diagnostic_environment_{std::move(diagnostic_environment)},
      workflow_{} {
  connect(&workflow_, &SupportBundleWorkflow::finished, this,
          &SupportBundleViewModel::handleFinished);
}

SupportBundleCommandResult SupportBundleViewModel::Export(
    std::filesystem::path destination, bool replace_existing) {
  if (busy_) {
    return std::unexpected{SupportBundleCommandError::kBusy};
  }

  SPDLOG_INFO("Diagnostic support bundle export started");
  spdlog::default_logger()->flush();
  result_.reset();
  busy_ = true;
  emit busyChanged();
  workflow_.Create(support::SupportBundleRequest{
      .path = std::move(destination),
      .log_directory = log_directory_,
      .environment = diagnostic_environment_,
      .replace_existing = replace_existing,
  });
  return {};
}

bool SupportBundleViewModel::IsBusy(void) const noexcept { return busy_; }

const support::CreateSupportBundleResult* SupportBundleViewModel::Result(
    void) const noexcept {
  return result_.has_value() ? &*result_ : nullptr;
}

void SupportBundleViewModel::handleFinished(void) {
  result_ = workflow_.Result();
  busy_ = false;
  emit busyChanged();

  if (!result_->has_value()) {
    SPDLOG_ERROR("Diagnostic support bundle export failed at stage {}: {}",
                 static_cast<int>(result_->error().kind),
                 result_->error().detail);
  } else {
    SPDLOG_INFO("Diagnostic support bundle exported with {} log file(s)",
                result_->value().log_file_count);
  }
  emit exportFinished();
}

}  // namespace edit_atlas::presentation
