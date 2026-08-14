#ifndef EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_VIEW_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_VIEW_MODEL_HPP_

#include <edit_atlas/presentation/support_bundle_workflow.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QObject>

#include <expected>
#include <filesystem>
#include <optional>

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
    SupportBundleViewModel(
        std::filesystem::path log_directory,
        support::DiagnosticEnvironment diagnostic_environment,
        QObject *parent = nullptr);
    ~SupportBundleViewModel(void) override = default;

    SupportBundleViewModel(const SupportBundleViewModel &) = delete;
    SupportBundleViewModel &operator=(const SupportBundleViewModel &) = delete;
    SupportBundleViewModel(SupportBundleViewModel &&) = delete;
    SupportBundleViewModel &operator=(SupportBundleViewModel &&) = delete;

    /// Starts an asynchronous export to the frontend-selected destination.
    [[nodiscard]] SupportBundleCommandResult
    Export(std::filesystem::path destination, bool replace_existing);

    /// Returns whether a support-bundle export is running.
    [[nodiscard]] bool IsBusy(void) const noexcept;
    /// Returns the most recent completed result, or null before completion.
    [[nodiscard]] const support::CreateSupportBundleResult *
    Result(void) const noexcept;

  signals:
    /// Reports a change to `IsBusy()`.
    void BusyChanged(void);
    /// Reports that a completed result is available.
    void ExportFinished(void);

  private:
    void HandleFinished(void);

    std::filesystem::path log_directory_;
    support::DiagnosticEnvironment diagnostic_environment_;
    SupportBundleWorkflow workflow_;
    std::optional<support::CreateSupportBundleResult> result_;
    bool busy_ = false;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_SUPPORT_BUNDLE_VIEW_MODEL_HPP_
