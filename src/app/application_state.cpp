#include <edit_atlas/app/application_state.hpp>

#include <QByteArray>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#include <cstddef>
#include <filesystem>
#include <optional>

namespace edit_atlas::app {
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

[[nodiscard]] std::optional<std::filesystem::path> TestStateRoot(void) {
    const auto value = qgetenv(kTestStateRootEnvironment);
    if (value.isEmpty()) {
        return std::nullopt;
    }
    return FilesystemPath(QString::fromUtf8(value));
}

} // namespace

void ConfigureApplicationState(void) {
    const auto root = TestStateRoot();
    if (!root.has_value()) {
        return;
    }

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       PathText(*root / "settings"));
}

std::filesystem::path ConfiguredApplicationDataDirectory(void) {
    if (const auto root = TestStateRoot(); root.has_value()) {
        return *root;
    }
    return FilesystemPath(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}

std::filesystem::path ConfiguredTemplateDirectory(void) {
    return ConfiguredApplicationDataDirectory() / "templates";
}

} // namespace edit_atlas::app
