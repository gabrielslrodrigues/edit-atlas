#include <edit_atlas/presentation/support_bundle_view_model.hpp>

#include <edit_atlas/presentation/support_bundle_workflow.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QObject>

#include <spdlog/spdlog.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <utility>

namespace edit_atlas::presentation {

SupportBundleViewModel::SupportBundleViewModel(
    std::filesystem::path log_directory,
    support::DiagnosticEnvironment diagnostic_environment, QObject *parent)
    : QObject{parent}, log_directory_{std::move(log_directory)},
      diagnostic_environment_{std::move(diagnostic_environment)}, workflow_{} {
    connect(&workflow_, &SupportBundleWorkflow::Finished, this,
            &SupportBundleViewModel::HandleFinished);
}

SupportBundleCommandResult
SupportBundleViewModel::Export(std::filesystem::path destination,
                               bool replace_existing) {
    if (busy_) {
        return std::unexpected{SupportBundleCommandError::kBusy};
    }

    SPDLOG_INFO("Diagnostic support bundle export started");
    spdlog::default_logger()->flush();
    result_.reset();
    busy_ = true;
    emit BusyChanged();
    workflow_.Create(support::SupportBundleRequest{
        .path = std::move(destination),
        .log_directory = log_directory_,
        .environment = diagnostic_environment_,
        .replace_existing = replace_existing,
    });
    return {};
}

bool SupportBundleViewModel::IsBusy(void) const noexcept {
    return busy_;
}

const support::CreateSupportBundleResult *
SupportBundleViewModel::Result(void) const noexcept {
    return result_.has_value() ? &*result_ : nullptr;
}

void SupportBundleViewModel::HandleFinished(void) {
    result_ = workflow_.Result();
    busy_ = false;
    emit BusyChanged();

    if (!result_->has_value()) {
        SPDLOG_ERROR("Diagnostic support bundle export failed at stage {}: {}",
                     static_cast<int>(result_->error().kind),
                     result_->error().detail);
    } else {
        SPDLOG_INFO("Diagnostic support bundle exported with {} log file(s)",
                    result_->value().log_file_count);
    }
    emit ExportFinished();
}

} // namespace edit_atlas::presentation
