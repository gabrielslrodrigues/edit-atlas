#include <edit_atlas/presentation/support_bundle_workflow.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrentRun>

#include <utility>

namespace edit_atlas::presentation {

SupportBundleWorkflow::SupportBundleWorkflow(QObject *parent)
    : QObject{parent} {
    connect(&watcher_,
            &QFutureWatcher<support::CreateSupportBundleResult>::finished, this,
            &SupportBundleWorkflow::Finished);
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

} // namespace edit_atlas::presentation
