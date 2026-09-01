#ifndef EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_WORKFLOW_HPP_
#define EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_WORKFLOW_HPP_

#include <edit_atlas/support/support_bundle.hpp>

#include <QFutureWatcher>
#include <QObject>

namespace edit_atlas::presentation {

/// Owns Qt asynchronous execution for diagnostic support-bundle creation.
class SupportBundleWorkflow final : public QObject {
    Q_OBJECT

  public:
    /// Creates an idle asynchronous support-bundle workflow.
    explicit SupportBundleWorkflow(QObject *parent = nullptr);
    /// Waits for owned asynchronous work before destruction.
    ~SupportBundleWorkflow(void) override;

    /// Workflows are non-copyable QObject owners.
    SupportBundleWorkflow(const SupportBundleWorkflow &) = delete;
    /// Workflows are non-copy-assignable QObject owners.
    SupportBundleWorkflow &operator=(const SupportBundleWorkflow &) = delete;
    /// Workflows are non-movable QObject owners.
    SupportBundleWorkflow(SupportBundleWorkflow &&) = delete;
    /// Workflows are non-move-assignable QObject owners.
    SupportBundleWorkflow &operator=(SupportBundleWorkflow &&) = delete;

    /// Starts support-bundle creation on a worker thread.
    void Create(support::SupportBundleRequest request);
    /// Returns the most recent completed support-bundle result.
    [[nodiscard]] support::CreateSupportBundleResult Result(void) const;
    /// Returns whether support-bundle creation is running.
    [[nodiscard]] bool IsBusy(void) const noexcept;

  signals:
    /// Reports that support-bundle creation finished.
    void finished(void);

  private:
    QFutureWatcher<support::CreateSupportBundleResult> watcher_;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_WORKFLOW_HPP_
