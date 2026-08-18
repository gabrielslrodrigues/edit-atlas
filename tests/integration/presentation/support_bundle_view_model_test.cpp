#include <edit_atlas/presentation/support_bundle_view_model.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>

namespace edit_atlas::presentation {
namespace {

[[nodiscard]] std::filesystem::path FilesystemPath(const QString &path) {
    const auto utf8 = path.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

[[nodiscard]] support::DiagnosticEnvironment Environment(void) {
    return support::DiagnosticEnvironment{
        .application_version = "test-version",
        .operating_system = "Test OS",
        .architecture = "test-architecture",
        .qt_version = "6.test",
        .platform_plugin = "test-platform",
        .importer_formats = {"cmx-3600"},
        .exporter_formats = {"xlsx"},
    };
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

TEST(SupportBundleViewModelTest, ExportsAndRetainsTheCompletedReceipt) {
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const auto directory = FilesystemPath(temporary.path());
    const auto logs = directory / "logs";
    std::filesystem::create_directories(logs);
    WriteFile(logs / "edit-atlas.log", "test log");
    const auto destination = directory / "diagnostics.zip";
    SupportBundleViewModel view_model{logs, Environment()};
    QSignalSpy busy_changed{&view_model, &SupportBundleViewModel::BusyChanged};
    QSignalSpy finished{&view_model, &SupportBundleViewModel::ExportFinished};

    ASSERT_TRUE(view_model.Export(destination, false).has_value());
    EXPECT_TRUE(view_model.IsBusy());
    EXPECT_EQ(view_model.Result(), nullptr);
    const auto duplicate_export =
        view_model.Export(directory / "duplicate.zip", false);
    ASSERT_FALSE(duplicate_export.has_value());
    EXPECT_EQ(duplicate_export.error(), SupportBundleCommandError::kBusy);
    ASSERT_TRUE(WaitForSignal(finished));

    EXPECT_FALSE(view_model.IsBusy());
    ASSERT_NE(view_model.Result(), nullptr);
    ASSERT_TRUE(view_model.Result()->has_value());
    EXPECT_EQ(view_model.Result()->value().path, destination);
    EXPECT_EQ(view_model.Result()->value().log_file_count, 1);
    EXPECT_TRUE(std::filesystem::exists(destination));
    EXPECT_GE(busy_changed.count(), 2);
}

TEST(SupportBundleViewModelTest, RetainsStructuredExportFailure) {
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const auto directory = FilesystemPath(temporary.path());
    const auto destination = directory / "existing.zip";
    WriteFile(destination, "preserve me");
    SupportBundleViewModel view_model{directory / "logs", Environment()};
    QSignalSpy finished{&view_model, &SupportBundleViewModel::ExportFinished};

    ASSERT_TRUE(view_model.Export(destination, false).has_value());
    ASSERT_TRUE(WaitForSignal(finished));

    ASSERT_NE(view_model.Result(), nullptr);
    ASSERT_FALSE(view_model.Result()->has_value());
    EXPECT_EQ(view_model.Result()->error().kind,
              support::SupportBundleFailureKind::kDestinationExists);
    EXPECT_EQ(view_model.Result()->error().path, destination);
}

} // namespace
} // namespace edit_atlas::presentation
