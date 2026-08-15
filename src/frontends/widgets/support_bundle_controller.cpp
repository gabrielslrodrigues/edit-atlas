#include <edit_atlas/frontends/widgets/support_bundle_controller.hpp>

#include "accessibility.hpp"

#include <edit_atlas/presentation/desktop_integration.hpp>
#include <edit_atlas/presentation/support_bundle_view_model.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>

namespace edit_atlas::frontends::widgets {
namespace {

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] std::filesystem::path FilesystemPath(const QString &path) {
    const auto utf8 = path.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

[[nodiscard]] QString PathText(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return QString::fromUtf8(reinterpret_cast<const char *>(utf8.data()),
                             static_cast<qsizetype>(utf8.size()));
}

} // namespace

SupportBundleController::SupportBundleController(
    std::filesystem::path log_directory,
    support::DiagnosticEnvironment diagnostic_environment, QWidget &window)
    : QObject{&window}, window_{window},
      view_model_{std::move(log_directory), std::move(diagnostic_environment)} {
    connect(&view_model_, &presentation::SupportBundleViewModel::BusyChanged,
            this, &SupportBundleController::HandleBusyChanged);
    connect(&view_model_, &presentation::SupportBundleViewModel::ExportFinished,
            this, &SupportBundleController::HandleFinished);
}

void SupportBundleController::ExportDiagnosticLogs(void) {
    if (!interactions_enabled_ || IsBusy()) {
        return;
    }

    QMessageBox disclosure{
        QMessageBox::Information,
        tr("Export Diagnostic Logs"),
        tr("The support bundle will contain recent Edit Atlas application "
           "logs and a summary of the application version, operating system, "
           "architecture, Qt version, platform plugin, and registered "
           "formats."),
        QMessageBox::NoButton,
        &window_,
    };
    disclosure.setInformativeText(
        tr("It will not automatically include timelines, spreadsheets, "
           "media, environment variables, or secrets."));
    auto *continue_button =
        disclosure.addButton(tr("Continue"), QMessageBox::AcceptRole);
    auto *cancel_button =
        disclosure.addButton(tr("Cancel"), QMessageBox::RejectRole);

    SetAutomationIdentifier(disclosure, u"supportBundleDisclosureDialog");
    SetAutomationIdentifier(*continue_button, u"continueSupportBundleButton");
    SetAutomationIdentifier(*cancel_button, u"cancelSupportBundleButton");

    disclosure.exec();
    if (disclosure.clickedButton() != continue_button) {
        return;
    }

    QFileDialog dialog{
        &window_,
        tr("Export Diagnostic Logs"),
        QStringLiteral("edit-atlas-diagnostics.zip"),
    };
    SetAutomationIdentifier(dialog, u"supportBundleSaveFileDialog");
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDefaultSuffix(QStringLiteral("zip"));
    dialog.setNameFilters({tr("ZIP archive (*.zip)"), tr("All files (*)")});
    dialog.setOption(QFileDialog::DontConfirmOverwrite, true);
    dialog.resize(1000, 650);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto selected_files = dialog.selectedFiles();
    if (selected_files.empty()) {
        return;
    }

    const auto destination = selected_files.front();
    const auto replace_existing = QFileInfo::exists(destination);
    if (replace_existing) {
        QMessageBox confirmation{
            QMessageBox::Question,
            tr("Replace Existing File?"),
            tr("%1 already exists. Do you want to replace it?")
                .arg(QFileInfo{destination}.fileName()),
            QMessageBox::NoButton,
            &window_,
        };
        auto *replace_button =
            confirmation.addButton(tr("Replace"), QMessageBox::AcceptRole);
        auto *cancel =
            confirmation.addButton(tr("Cancel"), QMessageBox::RejectRole);

        SetAutomationIdentifier(confirmation, u"replaceSupportBundleDialog");
        SetAutomationIdentifier(*replace_button, u"replaceSupportBundleButton");
        SetAutomationIdentifier(*cancel, u"cancelReplaceSupportBundleButton");

        confirmation.exec();
        if (confirmation.clickedButton() != replace_button) {
            return;
        }
    }

    static_cast<void>(
        view_model_.Export(FilesystemPath(destination), replace_existing));
}

bool SupportBundleController::IsBusy(void) const noexcept {
    return view_model_.IsBusy();
}

void SupportBundleController::RetranslateUi(void) {
    if (IsBusy()) {
        emit StatusMessageChanged(tr("Creating diagnostic support bundle…"));
    }
}

void SupportBundleController::SetInteractionsEnabled(bool enabled) {
    interactions_enabled_ = enabled;
}

void SupportBundleController::HandleBusyChanged(void) {
    const bool busy = view_model_.IsBusy();
    emit BusyChanged(busy);
    if (busy) {
        emit StatusMessageChanged(tr("Creating diagnostic support bundle…"));
    } else {
        emit StatusMessageCleared();
    }
}

void SupportBundleController::HandleFinished(void) {
    const auto *result = view_model_.Result();
    if (result == nullptr) {
        return;
    }
    if (!result->has_value()) {
        ShowFailure(result->error());
        return;
    }

    const auto &receipt = result->value();
    const auto path = PathText(receipt.path);
    QMessageBox message{
        QMessageBox::Information,
        tr("Diagnostic Logs Exported"),
        tr("The support bundle was saved to:\n%1").arg(path),
        QMessageBox::NoButton,
        &window_,
    };
    message.setInformativeText(
        tr("Recent application log files included: %1")
            .arg(static_cast<qulonglong>(receipt.log_file_count)));
    auto *reveal_button =
        message.addButton(tr("Reveal File"), QMessageBox::ActionRole);
    auto *close_button =
        message.addButton(tr("Close"), QMessageBox::RejectRole);

    SetAutomationIdentifier(message, u"supportBundleResultDialog");
    SetAutomationIdentifier(*reveal_button, u"revealSupportBundleButton");
    SetAutomationIdentifier(*close_button, u"closeDialogButton");

    message.exec();
    if (message.clickedButton() == reveal_button &&
        !presentation::desktop_integration::RevealFile(path)) {
        QMessageBox warning{
            QMessageBox::Warning,
            tr("Could Not Reveal File"),
            tr("The support bundle was saved, but its location could not be "
               "opened."),
            QMessageBox::NoButton,
            &window_,
        };
        auto *close = warning.addButton(tr("Close"), QMessageBox::RejectRole);

        SetAutomationIdentifier(warning, u"revealSupportBundleFailureDialog");
        SetAutomationIdentifier(*close, u"closeDialogButton");

        warning.exec();
    }
}

void SupportBundleController::ShowFailure(
    const support::SupportBundleFailure &failure) {
    QString description;
    switch (failure.kind) {
    case support::SupportBundleFailureKind::kDestinationExists:
        description =
            tr("The destination file already exists and was not replaced.");
        break;
    case support::SupportBundleFailureKind::kReadLogsFailed:
        description = tr("The application logs could not be read.");
        break;
    case support::SupportBundleFailureKind::kWriteBundleFailed:
        description = tr("The diagnostic support bundle could not be "
                         "created.");
        break;
    case support::SupportBundleFailureKind::kCommitFailed:
        description = tr("The completed support bundle could not replace the "
                         "destination file.");
        break;
    }
    if (failure.filesystem_error) {
        description +=
            QStringLiteral("\n") + Utf8(failure.filesystem_error.message());
    }

    QMessageBox message{
        QMessageBox::Critical,
        tr("Could Not Export Diagnostic Logs"),
        description,
        QMessageBox::NoButton,
        &window_,
    };
    auto *close = message.addButton(tr("Close"), QMessageBox::RejectRole);

    SetAutomationIdentifier(message, u"supportBundleFailureDialog");
    SetAutomationIdentifier(*close, u"closeDialogButton");

    message.exec();
}

} // namespace edit_atlas::frontends::widgets
