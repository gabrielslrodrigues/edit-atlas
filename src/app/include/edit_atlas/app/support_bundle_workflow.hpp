#ifndef EDIT_ATLAS_APP_SUPPORT_BUNDLE_WORKFLOW_HPP_
#define EDIT_ATLAS_APP_SUPPORT_BUNDLE_WORKFLOW_HPP_

#include <edit_atlas/support/support_bundle.hpp>

#include <QFutureWatcher>
#include <QObject>

namespace edit_atlas::app {

/// Owns Qt asynchronous execution for diagnostic support-bundle creation.
class SupportBundleWorkflow final : public QObject {
    Q_OBJECT

  public:
    explicit SupportBundleWorkflow(QObject *parent = nullptr);
    ~SupportBundleWorkflow(void) override;

    SupportBundleWorkflow(const SupportBundleWorkflow &) = delete;
    SupportBundleWorkflow &operator=(const SupportBundleWorkflow &) = delete;
    SupportBundleWorkflow(SupportBundleWorkflow &&) = delete;
    SupportBundleWorkflow &operator=(SupportBundleWorkflow &&) = delete;

    void Create(support::SupportBundleRequest request);
    [[nodiscard]] support::CreateSupportBundleResult Result(void) const;
    [[nodiscard]] bool IsBusy(void) const noexcept;

  signals:
    void Finished(void);

  private:
    QFutureWatcher<support::CreateSupportBundleResult> watcher_;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_SUPPORT_BUNDLE_WORKFLOW_HPP_
