#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_SUPPORT_BUNDLE_CONTROLLER_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_SUPPORT_BUNDLE_CONTROLLER_HPP_

#include <edit_atlas/presentation/support_bundle_view_model.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QObject>
#include <QString>

#include <filesystem>

class QWidget;

namespace edit_atlas::frontends::widgets {

/// Adapts support-bundle ViewModel commands to Qt Widgets interactions.
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
    void busyChanged(bool busy);
    void statusMessageChanged(const QString &message);
    void statusMessageCleared(void);

  private:
    void HandleBusyChanged(void);
    void HandleFinished(void);
    void ShowFailure(const support::SupportBundleFailure &failure);

    QWidget &window_;
    presentation::SupportBundleViewModel view_model_;
    bool interactions_enabled_ = true;
};

} // namespace edit_atlas::frontends::widgets

#endif // EDIT_ATLAS_FRONTENDS_WIDGETS_SUPPORT_BUNDLE_CONTROLLER_HPP_
