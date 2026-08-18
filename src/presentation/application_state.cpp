#include <edit_atlas/presentation/application_state.hpp>

#include <QByteArray>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtGlobal>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace edit_atlas::presentation {
namespace {

constexpr qsizetype kMaximumRecentFiles = 10;

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

bool RememberRecentFilesEnabled(void) {
    const QSettings settings;
    return settings.value(QStringLiteral("files/rememberRecent"), false)
        .toBool();
}

void SetRememberRecentFilesEnabled(bool enabled) {
    QSettings settings;
    settings.setValue(QStringLiteral("files/rememberRecent"), enabled);
    if (!enabled) {
        settings.remove(QStringLiteral("files/recent"));
    }
}

std::vector<std::filesystem::path> ConfiguredRecentFiles(void) {
    if (!RememberRecentFilesEnabled()) {
        return {};
    }

    const QSettings settings;
    const auto values =
        settings.value(QStringLiteral("files/recent")).toStringList();
    std::vector<std::filesystem::path> paths;
    paths.reserve(static_cast<std::size_t>(values.size()));
    for (const auto &value : values) {
        paths.emplace_back(FilesystemPath(value));
    }
    return paths;
}

void RecordRecentFile(const std::filesystem::path &path) {
    if (!RememberRecentFilesEnabled()) {
        return;
    }

    QSettings settings;
    auto recent_files =
        settings.value(QStringLiteral("files/recent")).toStringList();
    const auto path_text = PathText(path);
    recent_files.removeAll(path_text);
    recent_files.prepend(path_text);
    while (recent_files.size() > kMaximumRecentFiles) {
        recent_files.removeLast();
    }
    settings.setValue(QStringLiteral("files/recent"), recent_files);
}

} // namespace edit_atlas::presentation
