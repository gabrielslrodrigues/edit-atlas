#ifndef EDIT_ATLAS_APP_SUPPORT_BUNDLE_CONTROLLER_HPP_
#define EDIT_ATLAS_APP_SUPPORT_BUNDLE_CONTROLLER_HPP_

#include <edit_atlas/support/support_bundle.hpp>

#include <QObject>
#include <QString>

#include <filesystem>

class QWidget;

namespace edit_atlas::app {

class SupportBundleWorkflow;

/// Coordinates privacy disclosure and diagnostic support-bundle export.
class SupportBundleController final : public QObject {
    Q_OBJECT

  public:
    SupportBundleController(
        std::filesystem::path log_directory,
        support::DiagnosticEnvironment diagnostic_environment, QWidget &window);
    ~SupportBundleController(void) override = default;

    SupportBundleController(const SupportBundleController &) = delete;
    SupportBundleController &
    operator=(const SupportBundleController &) = delete;
    SupportBundleController(SupportBundleController &&) = delete;
    SupportBundleController &operator=(SupportBundleController &&) = delete;

    void ExportDiagnosticLogs(void);
    [[nodiscard]] bool IsBusy(void) const noexcept;
    void RetranslateUi(void);
    void SetInteractionsEnabled(bool enabled);

  signals:
    void BusyChanged(bool busy);
    void StatusMessageChanged(const QString &message);
    void StatusMessageCleared(void);

  private:
    void HandleFinished(void);
    void ShowFailure(const support::SupportBundleFailure &failure);

    std::filesystem::path log_directory_;
    support::DiagnosticEnvironment diagnostic_environment_;
    QWidget &window_;
    SupportBundleWorkflow *workflow_ = nullptr;
    bool interactions_enabled_ = true;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_SUPPORT_BUNDLE_CONTROLLER_HPP_
