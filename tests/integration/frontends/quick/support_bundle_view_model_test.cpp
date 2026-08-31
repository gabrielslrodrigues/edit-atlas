#include <edit_atlas/frontends/quick/support_bundle_view_model.hpp>

#include <edit_atlas/presentation/diagnostic_support.hpp>

#include <edit_atlas/services/built_in_formats.hpp>

#include <QDir>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>

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

void WriteFile(const std::filesystem::path &path, std::string_view content) {
    std::ofstream output{
        path,
        std::ios::binary | std::ios::out | std::ios::trunc,
    };
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

[[nodiscard]] bool WaitForSignal(QSignalSpy &spy) {
    return !spy.isEmpty() || spy.wait(5'000);
}

TEST(SupportBundleViewModelTest, ExportsConfiguredLogsAndRetainsTheResult) {
    const auto logs = presentation::ConfiguredLogDirectory();
    std::filesystem::create_directories(logs);
    WriteFile(logs / "edit-atlas.log", "quick frontend test log");

    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const auto destination =
        FilesystemPath(temporary.path()) / "diagnostics.zip";
    auto registry = services::CreateBuiltInFormatRegistry().value();
    SupportBundleViewModel support_bundle{registry};
    QSignalSpy succeeded{&support_bundle,
                         &SupportBundleViewModel::exportSucceeded};
    QSignalSpy result_changed{&support_bundle,
                              &SupportBundleViewModel::resultChanged};

    const auto destination_url = QUrl::fromLocalFile(PathText(destination));
    EXPECT_FALSE(support_bundle.DestinationExists(destination_url));
    ASSERT_TRUE(support_bundle.Start(destination_url, false));
    EXPECT_TRUE(support_bundle.IsBusy());
    ASSERT_TRUE(WaitForSignal(succeeded));

    EXPECT_FALSE(support_bundle.IsBusy());
    EXPECT_EQ(support_bundle.ResultPath(), PathText(destination));
    EXPECT_GE(support_bundle.IncludedLogFileCount(), 1U);
    EXPECT_TRUE(support_bundle.ErrorText().isEmpty());
    EXPECT_TRUE(std::filesystem::exists(destination));
    EXPECT_GE(result_changed.count(), 1);

    support_bundle.ClearResult();

    EXPECT_TRUE(support_bundle.ResultPath().isEmpty());
    EXPECT_EQ(support_bundle.IncludedLogFileCount(), 0U);
}

TEST(SupportBundleViewModelTest, RejectsNonlocalDestination) {
    auto registry = services::CreateBuiltInFormatRegistry().value();
    SupportBundleViewModel support_bundle{registry};
    QSignalSpy failed{&support_bundle, &SupportBundleViewModel::exportFailed};

    EXPECT_FALSE(support_bundle.Start(
        QUrl{QStringLiteral("https://example.com/diagnostics.zip")}, false));

    EXPECT_FALSE(support_bundle.IsBusy());
    EXPECT_FALSE(support_bundle.ErrorText().isEmpty());
    EXPECT_EQ(failed.count(), 1);
}

TEST(SupportBundleViewModelTest, RejectsAMissingDestinationDirectory) {
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const auto destination =
        FilesystemPath(temporary.path()) / "missing" / "diagnostics.zip";
    auto registry = services::CreateBuiltInFormatRegistry().value();
    SupportBundleViewModel support_bundle{registry};
    QSignalSpy failed{&support_bundle, &SupportBundleViewModel::exportFailed};

    EXPECT_FALSE(support_bundle.Start(
        QUrl::fromLocalFile(PathText(destination)), false));

    EXPECT_FALSE(support_bundle.IsBusy());
    EXPECT_FALSE(support_bundle.ErrorText().isEmpty());
    EXPECT_EQ(failed.count(), 1);
}

TEST(SupportBundleViewModelTest, FallsBackFromAMissingPersistedDirectory) {
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const auto missing_directory =
        QDir{temporary.path()}.filePath(QStringLiteral("missing"));
    QSettings settings;
    settings.setValue(QStringLiteral("supportBundle/lastDirectory"),
                      missing_directory);
    auto registry = services::CreateBuiltInFormatRegistry().value();
    SupportBundleViewModel support_bundle{registry};

    EXPECT_EQ(support_bundle.SuggestedDestinationUrl(),
              QUrl::fromLocalFile(QDir{QDir::homePath()}.filePath(
                  QStringLiteral("edit-atlas-diagnostics.zip"))));

    settings.remove(QStringLiteral("supportBundle/lastDirectory"));
}

TEST(SupportBundleViewModelTest, RequiresConsentBeforeReplacingAFile) {
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const auto destination =
        FilesystemPath(temporary.path()) / "diagnostics.zip";
    WriteFile(destination, "existing destination");
    const auto destination_url = QUrl::fromLocalFile(PathText(destination));
    auto registry = services::CreateBuiltInFormatRegistry().value();
    SupportBundleViewModel support_bundle{registry};
    QSignalSpy failed{&support_bundle, &SupportBundleViewModel::exportFailed};

    EXPECT_TRUE(support_bundle.DestinationExists(destination_url));
    EXPECT_FALSE(support_bundle.Start(destination_url, false));

    EXPECT_FALSE(support_bundle.IsBusy());
    EXPECT_FALSE(support_bundle.ErrorText().isEmpty());
    EXPECT_EQ(failed.count(), 1);
}

} // namespace
} // namespace edit_atlas::frontends::quick
