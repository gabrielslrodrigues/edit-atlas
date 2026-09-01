#include <edit_atlas/frontends/quick/support_bundle_view_model.hpp>

#include <edit_atlas/presentation/desktop_integration.hpp>
#include <edit_atlas/presentation/diagnostic_support.hpp>
#include <edit_atlas/presentation/support_bundle_view_model.hpp>

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QtGlobal>

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace edit_atlas::frontends::quick {
namespace {

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

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QString SuggestedDirectory(void) {
    const QSettings settings;
    const auto configured_directory =
        settings.value(QStringLiteral("supportBundle/lastDirectory"))
            .toString();
    const QDir directory{configured_directory};
    return !configured_directory.isEmpty() && directory.exists()
               ? directory.absolutePath()
               : QDir::homePath();
}

} // namespace

SupportBundleViewModel::SupportBundleViewModel(
    const core::FormatRegistry &registry, QObject *parent)
    : QObject{parent},
      view_model_{presentation::ConfiguredLogDirectory(),
                  presentation::CreateDiagnosticEnvironment(registry)} {
    connect(&view_model_, &presentation::SupportBundleViewModel::busyChanged,
            this, &SupportBundleViewModel::busyChanged);
    connect(&view_model_, &presentation::SupportBundleViewModel::exportFinished,
            this, &SupportBundleViewModel::HandleFinished);
}

bool SupportBundleViewModel::IsBusy(void) const noexcept {
    return view_model_.IsBusy();
}

QUrl SupportBundleViewModel::SuggestedDestinationUrl(void) const {
    return QUrl::fromLocalFile(QDir{SuggestedDirectory()}.filePath(
        QStringLiteral("edit-atlas-diagnostics.zip")));
}

QString SupportBundleViewModel::ResultPath(void) const {
    return result_path_;
}

qulonglong SupportBundleViewModel::IncludedLogFileCount(void) const noexcept {
    return included_log_file_count_;
}

QString SupportBundleViewModel::ErrorText(void) const {
    return error_text_;
}

bool SupportBundleViewModel::DestinationExists(const QUrl &destination) const {
    const QFileInfo destination_info{destination.toLocalFile()};
    return destination.isLocalFile() && destination_info.exists() &&
           !destination_info.isDir();
}

bool SupportBundleViewModel::Start(const QUrl &destination,
                                   bool replace_existing) {
    ClearResult();
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty()) {
        SetImmediateFailure(
            tr("Select a local destination for the support bundle."));
        return false;
    }

    const QFileInfo destination_info{destination.toLocalFile()};
    if (destination_info.isDir()) {
        SetImmediateFailure(
            tr("Select a file destination for the support bundle."));
        return false;
    }
    if (!destination_info.absoluteDir().exists()) {
        SetImmediateFailure(
            tr("The selected destination directory does not exist."));
        return false;
    }

    const auto destination_path = FilesystemPath(destination_info.filePath());
    std::error_code exists_error;
    const auto destination_exists =
        std::filesystem::exists(destination_path, exists_error);
    if (exists_error) {
        SetImmediateFailure(
            tr("The support-bundle destination could not be read."));
        return false;
    }
    if (destination_exists && !replace_existing) {
        SetImmediateFailure(
            tr("The destination file already exists and was not replaced."));
        return false;
    }

    if (!view_model_.Export(destination_path, replace_existing).has_value()) {
        SetImmediateFailure(
            tr("The support-bundle export could not be started."));
        return false;
    }
    return true;
}

bool SupportBundleViewModel::RevealResult(void) const {
    return !result_path_.isEmpty() &&
           presentation::desktop_integration::RevealFile(result_path_);
}

void SupportBundleViewModel::ClearResult(void) {
    if (result_path_.isEmpty() && error_text_.isEmpty() &&
        included_log_file_count_ == 0) {
        return;
    }
    result_path_.clear();
    error_text_.clear();
    included_log_file_count_ = 0;
    emit resultChanged();
}

void SupportBundleViewModel::HandleFinished(void) {
    const auto *result = view_model_.Result();
    if (result == nullptr) {
        return;
    }
    if (result->has_value()) {
        result_path_ = PathText(result->value().path);
        included_log_file_count_ =
            static_cast<qulonglong>(result->value().log_file_count);
        QSettings settings;
        settings.setValue(QStringLiteral("supportBundle/lastDirectory"),
                          QFileInfo{result_path_}.absolutePath());
        emit suggestedDestinationChanged();
        emit resultChanged();
        emit exportSucceeded();
        return;
    }

    const auto &failure = result->error();
    switch (failure.kind) {
    case support::SupportBundleFailureKind::kDestinationExists:
        error_text_ =
            tr("The destination file already exists and was not replaced.");
        break;
    case support::SupportBundleFailureKind::kReadLogsFailed:
        error_text_ = tr("The application logs could not be read.");
        break;
    case support::SupportBundleFailureKind::kWriteBundleFailed:
        error_text_ = tr("The diagnostic support bundle could not be created.");
        break;
    case support::SupportBundleFailureKind::kCommitFailed:
        error_text_ = tr("The completed support bundle could not replace the "
                         "destination file.");
        break;
    }
    if (failure.filesystem_error) {
        error_text_ +=
            QStringLiteral("\n") + Utf8(failure.filesystem_error.message());
    } else if (!failure.detail.empty()) {
        error_text_ += QStringLiteral("\n") + Utf8(failure.detail);
    }
    emit resultChanged();
    emit exportFailed();
}

void SupportBundleViewModel::SetImmediateFailure(QString error) {
    error_text_ = std::move(error);
    emit resultChanged();
    emit exportFailed();
}

} // namespace edit_atlas::frontends::quick
