#include <edit_atlas/frontends/quick/application_shell_view_model.hpp>

#include <edit_atlas/presentation/application_state.hpp>
#include <edit_atlas/presentation/translation.hpp>

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/built_in_formats.hpp>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QUrl>
#include <QtGlobal>

#include <gtest/gtest.h>

#include <filesystem>
#include <functional>

namespace edit_atlas::frontends::quick {
namespace {

[[nodiscard]] core::FormatRegistry BuiltInRegistry(void) {
    return services::CreateBuiltInFormatRegistry().value();
}

[[nodiscard]] std::filesystem::path FixturePath(void) {
    return std::filesystem::path{EDIT_ATLAS_QUICK_FIXTURE_DIRECTORY} /
           "mixed_tracks.edl";
}

[[nodiscard]] QString PathText(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return QString::fromUtf8(reinterpret_cast<const char *>(utf8.data()),
                             static_cast<qsizetype>(utf8.size()));
}

[[nodiscard]] bool WaitUntil(const std::function<bool(void)> &condition,
                             int timeout_milliseconds = 5'000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout_milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return condition();
}

class ApplicationShellViewModelTest : public ::testing::Test {
  protected:
    void SetUp(void) override {
        QSettings settings;
        settings.clear();
        settings.sync();
    }
};

TEST_F(ApplicationShellViewModelTest,
       ExposesInitialStateAndRegisteredImportFormats) {
    auto registry = BuiltInRegistry();
    QTranslator translator;
    ApplicationShellViewModel shell{
        registry, translator, presentation::ApplicationLanguage::kEnglish};

    EXPECT_EQ(shell.CurrentDocumentState(),
              ApplicationShellViewModel::DocumentState::kEmpty);
    EXPECT_FALSE(shell.IsBusy());
    EXPECT_TRUE(shell.RequestClose());
    EXPECT_EQ(shell.EventCount(), 0U);
    EXPECT_EQ(shell.VisibleEventCount(), 0U);
    EXPECT_TRUE(shell.TimelineTitle().isEmpty());
    EXPECT_TRUE(shell.TimelineSummaryText().isEmpty());
    EXPECT_EQ(shell.EventModel()->rowCount(), 0);
    EXPECT_EQ(shell.DiagnosticsModel()->rowCount(), 0);
    EXPECT_EQ(shell.DiagnosticCount(), 0);
    EXPECT_EQ(shell.EventSortColumn(), -1);
    EXPECT_TRUE(shell.SourceFileName().isEmpty());
    EXPECT_EQ(shell.StatusText(), QStringLiteral("Ready"));
    EXPECT_TRUE(shell.ErrorText().isEmpty());
    EXPECT_EQ(shell.LanguageCode(), QStringLiteral("en"));
    EXPECT_TRUE(shell.RecentFiles().isEmpty());
    EXPECT_TRUE(shell.ImportFilePatterns().contains(QStringLiteral("*.edl")));
    EXPECT_EQ(shell.FileName(QStringLiteral("/timeline/example.edl")),
              QStringLiteral("example.edl"));
}

TEST_F(ApplicationShellViewModelTest, SharesPersistentDesktopPreferences) {
    auto registry = BuiltInRegistry();
    QTranslator translator;
    ApplicationShellViewModel shell{
        registry, translator, presentation::ApplicationLanguage::kEnglish};
    QSignalSpy remember_changed{
        &shell, &ApplicationShellViewModel::RememberRecentFilesChanged};
    QSignalSpy recent_files_changed{
        &shell, &ApplicationShellViewModel::RecentFilesChanged};
    QSignalSpy language_changed{&shell,
                                &ApplicationShellViewModel::LanguageChanged};

    shell.SetRememberRecentFiles(true);
    presentation::RecordRecentFile("/shared/example.edl");
    shell.SetLanguageCode(QStringLiteral("pt_BR"));

    EXPECT_TRUE(shell.RememberRecentFiles());
    EXPECT_EQ(shell.RecentFiles(),
              QStringList{QStringLiteral("/shared/example.edl")});
    EXPECT_EQ(shell.LanguageCode(), QStringLiteral("pt_BR"));
    EXPECT_EQ(presentation::ConfiguredApplicationLanguage(),
              presentation::ApplicationLanguage::kBrazilianPortuguese);
    EXPECT_EQ(remember_changed.count(), 1);
    EXPECT_EQ(recent_files_changed.count(), 1);
    EXPECT_EQ(language_changed.count(), 1);

    shell.SetRememberRecentFiles(false);

    EXPECT_FALSE(shell.RememberRecentFiles());
    EXPECT_TRUE(shell.RecentFiles().isEmpty());
    EXPECT_EQ(remember_changed.count(), 2);
    EXPECT_EQ(recent_files_changed.count(), 2);
}

TEST_F(ApplicationShellViewModelTest,
       RequestsMissingFrameRateAndCompletesTheRetry) {
    auto registry = BuiltInRegistry();
    QTranslator translator;
    ApplicationShellViewModel shell{
        registry, translator, presentation::ApplicationLanguage::kEnglish};
    QSignalSpy frame_rate_required{
        &shell, &ApplicationShellViewModel::frameRateRequired};
    QSignalSpy document_state_changed{
        &shell, &ApplicationShellViewModel::DocumentStateChanged};
    shell.SetRememberRecentFiles(true);
    QSignalSpy recent_files_changed{
        &shell, &ApplicationShellViewModel::RecentFilesChanged};

    const auto fixture = FixturePath();
    shell.OpenUrl(QUrl::fromLocalFile(PathText(fixture)));

    EXPECT_TRUE(shell.IsBusy());
    EXPECT_FALSE(shell.RequestClose());
    ASSERT_TRUE(WaitUntil([&shell] {
        return shell.CurrentDocumentState() ==
               ApplicationShellViewModel::DocumentState::kImportFailed;
    }));
    EXPECT_FALSE(shell.IsBusy());
    EXPECT_TRUE(shell.RequestClose());
    EXPECT_EQ(frame_rate_required.count(), 1);
    EXPECT_FALSE(shell.ErrorText().isEmpty());
    EXPECT_EQ(shell.DiagnosticsModel()->rowCount(), shell.DiagnosticCount());

    shell.RetryWithFrameRate(QStringLiteral("24"));

    ASSERT_TRUE(WaitUntil([&shell] {
        return shell.CurrentDocumentState() ==
               ApplicationShellViewModel::DocumentState::kReady;
    }));
    EXPECT_EQ(shell.SourceFileName(), QStringLiteral("mixed_tracks.edl"));
    EXPECT_EQ(shell.EventCount(), 4U);
    EXPECT_EQ(shell.VisibleEventCount(), 4U);
    EXPECT_EQ(shell.TimelineTitle(), QStringLiteral("SYNTHETIC MIXED TRACKS"));
    EXPECT_EQ(shell.TimelineSummaryText(),
              QStringLiteral("4 events · 24 fps · non-drop-frame"));
    ASSERT_EQ(shell.EventModel()->rowCount(), 4);
    EXPECT_EQ(shell.StatusText(), QStringLiteral("Loaded mixed_tracks.edl"));
    EXPECT_EQ(shell.RecentFiles(), QStringList{PathText(fixture)});
    EXPECT_GE(document_state_changed.count(), 4);
    EXPECT_EQ(recent_files_changed.count(), 1);

    shell.ToggleEventSort(0);
    EXPECT_EQ(shell.EventSortColumn(), 0);
    EXPECT_TRUE(shell.EventSortAscending());
    EXPECT_EQ(shell.EventModel()->data(shell.EventModel()->index(0, 0))
                  .toString(),
              QStringLiteral("001"));

    shell.ToggleEventSort(0);
    EXPECT_FALSE(shell.EventSortAscending());
    EXPECT_EQ(shell.EventModel()->data(shell.EventModel()->index(0, 0))
                  .toString(),
              QStringLiteral("004"));
}

TEST_F(ApplicationShellViewModelTest, IgnoresUnsupportedOpenRequests) {
    auto registry = BuiltInRegistry();
    QTranslator translator;
    ApplicationShellViewModel shell{
        registry, translator, presentation::ApplicationLanguage::kEnglish};
    QSignalSpy document_state_changed{
        &shell, &ApplicationShellViewModel::DocumentStateChanged};

    shell.OpenUrl(QUrl{QStringLiteral("https://example.com/timeline.edl")});
    shell.OpenPath(QString{});
    shell.RetryWithFrameRate(QStringLiteral("24"));

    EXPECT_EQ(shell.CurrentDocumentState(),
              ApplicationShellViewModel::DocumentState::kEmpty);
    EXPECT_EQ(document_state_changed.count(), 0);
}

} // namespace
} // namespace edit_atlas::frontends::quick
