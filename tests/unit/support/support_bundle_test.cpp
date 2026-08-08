#include <edit_atlas/support/support_bundle.hpp>

#include <minizip/unzip.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace edit_atlas::support {
namespace {

class TemporaryDirectory final {
  public:
    explicit TemporaryDirectory(std::string_view name)
        : path_(std::filesystem::path{testing::TempDir()} / std::string{name}) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove_all(path_, error));
        std::filesystem::create_directories(path_, error);
        valid_ = !error;
    }

    ~TemporaryDirectory(void) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove_all(path_, error));
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;
    TemporaryDirectory(TemporaryDirectory &&) = delete;
    TemporaryDirectory &operator=(TemporaryDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

    [[nodiscard]] bool valid(void) const noexcept { return valid_; }

  private:
    std::filesystem::path path_;
    bool valid_ = false;
};

void WriteFile(const std::filesystem::path &path, std::string_view content) {
    std::ofstream output{
        path,
        std::ios::binary | std::ios::out | std::ios::trunc,
    };
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
}

[[nodiscard]] DiagnosticEnvironment Environment(void) {
    return DiagnosticEnvironment{
        .application_version = "0.1.0",
        .operating_system = "Test OS",
        .architecture = "test-architecture",
        .qt_version = "6.test",
        .platform_plugin = "test-platform",
        .importer_formats = {"cmx3600"},
        .exporter_formats = {"xlsx"},
    };
}

[[nodiscard]] std::vector<std::string>
ZipEntryNames(const std::filesystem::path &path) {
    const auto path_text = path.string();
    auto *archive = unzOpen64(path_text.c_str());
    if (archive == nullptr) {
        return {};
    }

    std::vector<std::string> names;
    if (unzGoToFirstFile(archive) == UNZ_OK) {
        do {
            std::array<char, 512> name{};
            if (unzGetCurrentFileInfo64(archive, nullptr, name.data(),
                                        static_cast<unsigned long>(name.size()),
                                        nullptr, 0, nullptr, 0) != UNZ_OK) {
                names.clear();
                break;
            }
            names.emplace_back(name.data());
        } while (unzGoToNextFile(archive) == UNZ_OK);
    }
    static_cast<void>(unzClose(archive));
    std::ranges::sort(names);
    return names;
}

[[nodiscard]] std::optional<std::string>
ReadZipEntry(const std::filesystem::path &path, std::string_view entry_name) {
    const auto path_text = path.string();
    auto *archive = unzOpen64(path_text.c_str());
    if (archive == nullptr) {
        return std::nullopt;
    }
    const std::string name{entry_name};
    if (unzLocateFile(archive, name.c_str(), 0) != UNZ_OK ||
        unzOpenCurrentFile(archive) != UNZ_OK) {
        static_cast<void>(unzClose(archive));
        return std::nullopt;
    }

    std::string content;
    std::array<char, 4'096> buffer{};
    int bytes_read = 0;
    while ((bytes_read = unzReadCurrentFile(
                archive, buffer.data(),
                static_cast<unsigned int>(buffer.size()))) > 0) {
        content.append(buffer.data(), static_cast<std::size_t>(bytes_read));
    }
    const auto file_close = unzCloseCurrentFile(archive);
    const auto archive_close = unzClose(archive);
    if (bytes_read < 0 || file_close != UNZ_OK || archive_close != UNZ_OK) {
        return std::nullopt;
    }
    return content;
}

TEST(SupportBundleTest, FormatsOnlyDocumentedEnvironmentMetadata) {
    const auto summary = FormatDiagnosticEnvironment(Environment());

    EXPECT_NE(summary.find("Application version: 0.1.0"), std::string::npos);
    EXPECT_NE(summary.find("Operating system: Test OS"), std::string::npos);
    EXPECT_NE(summary.find("Architecture: test-architecture"),
              std::string::npos);
    EXPECT_NE(summary.find("Qt version: 6.test"), std::string::npos);
    EXPECT_NE(summary.find("Qt platform plugin: test-platform"),
              std::string::npos);
    EXPECT_NE(summary.find("cmx3600"), std::string::npos);
    EXPECT_NE(summary.find("xlsx"), std::string::npos);
    EXPECT_NE(summary.find("does not automatically include"),
              std::string::npos);
}

TEST(SupportBundleTest, IncludesOnlyEnvironmentSummaryAndRecognizedLogs) {
    const TemporaryDirectory temporary{"support-bundle-contents"};
    ASSERT_TRUE(temporary.valid());
    const auto logs = temporary.path() / "logs";
    std::filesystem::create_directories(logs);
    WriteFile(logs / "edit-atlas.log", "current log");
    WriteFile(logs / "edit-atlas.1.log", "rotated log");
    WriteFile(logs / "timeline.edl", "PRIVATE TIMELINE CONTENT");
    WriteFile(logs / "secret.env", "SECRET_ENVIRONMENT_VALUE");
    const auto destination = temporary.path() / "support.zip";

    const auto result = CreateSupportBundle(SupportBundleRequest{
        .path = destination,
        .log_directory = logs,
        .environment = Environment(),
        .replace_existing = false,
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->log_file_count, 2);
    EXPECT_EQ(ZipEntryNames(destination), (std::vector<std::string>{
                                              "environment.txt",
                                              "logs/edit-atlas.1.log",
                                              "logs/edit-atlas.log",
                                          }));
    const auto summary = ReadZipEntry(destination, "environment.txt");
    ASSERT_TRUE(summary.has_value());
    EXPECT_EQ(summary->find("PRIVATE TIMELINE CONTENT"), std::string::npos);
    EXPECT_EQ(summary->find("SECRET_ENVIRONMENT_VALUE"), std::string::npos);
}

TEST(SupportBundleTest, PreservesExistingBundleWithoutPermission) {
    const TemporaryDirectory temporary{"support-bundle-existing"};
    ASSERT_TRUE(temporary.valid());
    const auto destination = temporary.path() / "support.zip";
    WriteFile(destination, "original");

    const auto result = CreateSupportBundle(SupportBundleRequest{
        .path = destination,
        .log_directory = temporary.path() / "missing-logs",
        .environment = Environment(),
        .replace_existing = false,
    });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind,
              SupportBundleFailureKind::kDestinationExists);
    EXPECT_EQ(ReadFile(destination), "original");
}

TEST(SupportBundleTest, SupportsUnicodeDestinationPaths) {
    const TemporaryDirectory temporary{"support-bundle-unicode"};
    ASSERT_TRUE(temporary.valid());
    const auto destination =
        temporary.path() / std::filesystem::path{u8"diagnóstico.zip"};

    const auto result = CreateSupportBundle(SupportBundleRequest{
        .path = destination,
        .log_directory = temporary.path() / "missing-logs",
        .environment = Environment(),
        .replace_existing = false,
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(destination));
}

} // namespace
} // namespace edit_atlas::support
