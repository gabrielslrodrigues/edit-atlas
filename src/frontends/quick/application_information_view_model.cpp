#include <edit_atlas/frontends/quick/application_information_view_model.hpp>

#include <edit_atlas/presentation/desktop_integration.hpp>
#include <edit_atlas/presentation/diagnostic_support.hpp>

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/media/video_decoder.hpp>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

namespace edit_atlas::frontends::quick {
namespace {

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QString PathText(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return QString::fromUtf8(reinterpret_cast<const char *>(utf8.data()),
                             static_cast<qsizetype>(utf8.size()));
}

[[nodiscard]] QStringList
FormatIdentifiers(const std::vector<std::string> &identifiers) {
    QStringList result;
    result.reserve(static_cast<qsizetype>(identifiers.size()));
    std::ranges::transform(
        identifiers, std::back_inserter(result),
        [](const std::string &identifier) { return Utf8(identifier); });
    return result;
}

} // namespace

ApplicationInformationViewModel::ApplicationInformationViewModel(
    const core::FormatRegistry &registry, QObject *parent)
    : QObject{parent} {
    const auto environment =
        presentation::CreateDiagnosticEnvironment(registry);
    const auto video_backend = media::GetVideoBackendInformation();
    application_version_ = Utf8(environment.application_version);
    operating_system_ = Utf8(environment.operating_system);
    architecture_ = Utf8(environment.architecture);
    qt_version_ = Utf8(environment.qt_version);
    platform_plugin_ = Utf8(environment.platform_plugin);
    video_backend_name_ = Utf8(video_backend.name);
    video_backend_version_ = Utf8(video_backend.version);
    video_backend_license_ = Utf8(video_backend.license);
    video_backend_configuration_ = Utf8(video_backend.configuration);
    import_formats_ = FormatIdentifiers(environment.importer_formats);
    export_formats_ = FormatIdentifiers(environment.exporter_formats);
    log_directory_ = PathText(presentation::ConfiguredLogDirectory());
}

QString ApplicationInformationViewModel::ApplicationVersion(void) const {
    return application_version_;
}

QString ApplicationInformationViewModel::OperatingSystem(void) const {
    return operating_system_;
}

QString ApplicationInformationViewModel::Architecture(void) const {
    return architecture_;
}

QString ApplicationInformationViewModel::QtVersion(void) const {
    return qt_version_;
}

QString ApplicationInformationViewModel::PlatformPlugin(void) const {
    return platform_plugin_;
}

QString ApplicationInformationViewModel::VideoBackendName(void) const {
    return video_backend_name_;
}

QString ApplicationInformationViewModel::VideoBackendVersion(void) const {
    return video_backend_version_;
}

QString ApplicationInformationViewModel::VideoBackendLicense(void) const {
    return video_backend_license_;
}

QString ApplicationInformationViewModel::VideoBackendConfiguration(void) const {
    return video_backend_configuration_;
}

QStringList ApplicationInformationViewModel::ImportFormats(void) const {
    return import_formats_;
}

QStringList ApplicationInformationViewModel::ExportFormats(void) const {
    return export_formats_;
}

QString ApplicationInformationViewModel::LogDirectory(void) const {
    return log_directory_;
}

bool ApplicationInformationViewModel::OpenLogDirectory(void) const {
    return presentation::desktop_integration::OpenDirectory(log_directory_);
}

bool ApplicationInformationViewModel::OpenProjectWebsite(void) const {
    return presentation::desktop_integration::OpenExternalUrl(QUrl{
        QStringLiteral("https://github.com/gabrielslrodrigues/edit-atlas")});
}

bool ApplicationInformationViewModel::OpenQtLicensingInformation(void) const {
    return presentation::desktop_integration::OpenExternalUrl(
        QUrl{QStringLiteral(
            "https://www.qt.io/development/open-source-lgpl-obligations")});
}

bool ApplicationInformationViewModel::OpenFfmpegLegalInformation(void) const {
    return presentation::desktop_integration::OpenExternalUrl(
        QUrl{QStringLiteral("https://ffmpeg.org/legal.html")});
}

} // namespace edit_atlas::frontends::quick
