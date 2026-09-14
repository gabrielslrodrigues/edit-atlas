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

#include "edit_atlas/presentation/support_bundle_workflow.hpp"

#include <utility>

#include "QFutureWatcher"
#include "QObject"
#include "QtConcurrentRun"

#include "edit_atlas/support/support_bundle.hpp"

namespace edit_atlas::presentation {

SupportBundleWorkflow::SupportBundleWorkflow(QObject* parent)
    : QObject{parent} {
  connect(&watcher_,
          &QFutureWatcher<support::CreateSupportBundleResult>::finished, this,
          &SupportBundleWorkflow::finished);
}

SupportBundleWorkflow::~SupportBundleWorkflow(void) {
  if (watcher_.isRunning()) {
    watcher_.waitForFinished();
  }
}

void SupportBundleWorkflow::Create(support::SupportBundleRequest request) {
  watcher_.setFuture(
      QtConcurrent::run([request = std::move(request)](void) mutable {
        return support::CreateSupportBundle(std::move(request));
      }));
}

support::CreateSupportBundleResult SupportBundleWorkflow::Result(void) const {
  return watcher_.result();
}

bool SupportBundleWorkflow::IsBusy(void) const noexcept {
  return watcher_.isRunning();
}

}  // namespace edit_atlas::presentation
