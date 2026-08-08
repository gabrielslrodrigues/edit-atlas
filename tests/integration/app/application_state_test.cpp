#include <edit_atlas/app/application_menu_bar.hpp>
#include <edit_atlas/app/application_state.hpp>
#include <edit_atlas/app/diagnostic_support.hpp>
#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_template_service.hpp>

#include <QAction>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <vector>

namespace edit_atlas::app {
namespace {

[[nodiscard]] std::filesystem::path TestStateRoot(void) {
    const auto value = qgetenv(kTestStateRootEnvironment);
    const auto text = QString::fromUtf8(value);
    const auto utf8 = text.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

[[nodiscard]] std::filesystem::path FilesystemPath(const QString &path) {
    const auto utf8 = path.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

TEST(ApplicationStateTest, RoutesEveryPersistentStoreBelowTheOverride) {
    const auto root = TestStateRoot();
    ASSERT_FALSE(root.empty());

    EXPECT_EQ(ConfiguredApplicationDataDirectory(), root);
    EXPECT_EQ(ConfiguredTemplateDirectory(), root / "templates");
    EXPECT_EQ(ConfiguredLogDirectory(), root / "logs");

    QSettings settings;
    settings.clear();
    SaveApplicationLanguage(ApplicationLanguage::kEnglish);
    settings.sync();

    EXPECT_EQ(ConfiguredApplicationLanguage(), ApplicationLanguage::kEnglish);
    EXPECT_EQ(settings.format(), QSettings::IniFormat);
    EXPECT_TRUE(FilesystemPath(settings.fileName())
                    .lexically_normal()
                    .string()
                    .starts_with((root / "settings").string()));
}

TEST(ApplicationStateTest, IsolatesRecentFilesAndTimelineTemplates) {
    QSettings settings;
    settings.clear();
    ApplicationMenuBar menu{ApplicationLanguage::kEnglish};
    auto *remember =
        menu.findChild<QAction *>(QStringLiteral("rememberRecentFilesAction"));
    ASSERT_NE(remember, nullptr);
    remember->setChecked(true);
    menu.RememberRecentFile(QStringLiteral("/isolated/example.edl"));
    settings.sync();

    EXPECT_TRUE(
        settings.value(QStringLiteral("files/rememberRecent")).toBool());
    EXPECT_EQ(settings.value(QStringLiteral("files/recent")).toStringList(),
              QStringList{QStringLiteral("/isolated/example.edl")});

    services::TimelineTemplateService templates{ConfiguredTemplateDirectory()};
    ASSERT_TRUE(templates.Load().has_value());
    const auto projection = core::DefaultTimelineEventProjection();
    const auto created = templates.Create(
        "Integration template", services::TimelineFilterQuery{},
        std::vector<core::TimelineEventField>{projection.begin(),
                                              projection.end()});
    ASSERT_TRUE(created.has_value());
    EXPECT_TRUE(std::filesystem::exists(ConfiguredTemplateDirectory() /
                                        (created->identifier + ".json")));
}

} // namespace
} // namespace edit_atlas::app
