#include <edit_atlas/support/application_logging.hpp>

#include <spdlog/spdlog.h>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace edit_atlas::support {
namespace {

class TemporaryDirectory final {
  public:
    explicit TemporaryDirectory(std::filesystem::path name)
        : path_(std::filesystem::path{testing::TempDir()} / std::move(name)) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove_all(path_, error));
        std::filesystem::create_directories(path_, error);
        valid_ = !error;
    }

    ~TemporaryDirectory(void) {
        spdlog::shutdown();
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

[[nodiscard]] LoggingOptions Options(const std::filesystem::path &directory) {
    return LoggingOptions{
        .directory = directory,
        .maximum_file_size = 256,
        .maximum_files = 3,
        .maximum_age = std::chrono::hours{24 * 14},
    };
}

TEST(ApplicationLoggingTest, BuildsPrivateLogDirectory) {
    const std::filesystem::path application_data{"/application-data"};

    EXPECT_EQ(ApplicationLogDirectory(application_data),
              application_data / "logs");
    EXPECT_TRUE(ApplicationLogDirectory({}).empty());
}

TEST(ApplicationLoggingTest, RecognizesOnlyApplicationLogFilenames) {
    EXPECT_TRUE(IsApplicationLogFilename("edit-atlas.log"));
    EXPECT_TRUE(IsApplicationLogFilename("edit-atlas.1.log"));
    EXPECT_TRUE(IsApplicationLogFilename("edit-atlas.42.log"));
    EXPECT_FALSE(IsApplicationLogFilename("edit-atlas.0.log"));
    EXPECT_FALSE(IsApplicationLogFilename("edit-atlas.01.log"));
    EXPECT_FALSE(IsApplicationLogFilename("edit-atlas.txt"));
    EXPECT_FALSE(IsApplicationLogFilename("edit-atlas.backup.log"));
    EXPECT_FALSE(IsApplicationLogFilename("another-app.log"));
}

TEST(ApplicationLoggingTest, PersistsAndBoundsRotatingLogs) {
    const TemporaryDirectory temporary{"application-logging-rotation"};
    ASSERT_TRUE(temporary.valid());
    const auto options = Options(temporary.path());
    const auto result = InitializeApplicationLogging(options);
    ASSERT_TRUE(result.has_value());

    for (std::size_t index = 0; index < 100; ++index) {
        SPDLOG_INFO("Bounded persistent log record {} with padding padding "
                    "padding padding",
                    index);
    }
    spdlog::default_logger()->flush();
    spdlog::shutdown();

    std::size_t log_count = 0;
    std::string combined_logs;
    for (const auto &entry :
         std::filesystem::directory_iterator{temporary.path()}) {
        if (IsApplicationLogFilename(entry.path().filename().string())) {
            ++log_count;
            combined_logs += ReadFile(entry.path());
        }
    }
    EXPECT_LE(log_count, options.maximum_files);
    EXPECT_FALSE(combined_logs.empty());
    EXPECT_NE(combined_logs.find("[application_logging_test.cpp:"),
              std::string::npos);
}

TEST(ApplicationLoggingTest, SupportsUnicodeLogPaths) {
    const TemporaryDirectory temporary{
        std::filesystem::path{u8"application-logging-diagnóstico"}};
    ASSERT_TRUE(temporary.valid());

    const auto result = InitializeApplicationLogging(Options(temporary.path()));
    ASSERT_TRUE(result.has_value());
    SPDLOG_INFO("Unicode path logging");
    spdlog::default_logger()->flush();
    spdlog::shutdown();

    EXPECT_TRUE(std::filesystem::exists(*result));
}

TEST(ApplicationLoggingTest, RemovesExpiredApplicationLogsOnly) {
    const TemporaryDirectory temporary{"application-logging-retention"};
    ASSERT_TRUE(temporary.valid());
    const auto expired_log = temporary.path() / "edit-atlas.2.log";
    const auto excess_rotation = temporary.path() / "edit-atlas.99.log";
    const auto unrelated_file = temporary.path() / "notes.txt";
    WriteFile(expired_log, "expired");
    WriteFile(excess_rotation, "excess");
    WriteFile(unrelated_file, "keep");
    const auto old_time = std::filesystem::file_time_type::clock::now() -
                          std::chrono::hours{24 * 30};
    std::error_code error;
    std::filesystem::last_write_time(expired_log, old_time, error);
    ASSERT_FALSE(error);
    std::filesystem::last_write_time(unrelated_file, old_time, error);
    ASSERT_FALSE(error);

    const auto result = InitializeApplicationLogging(Options(temporary.path()));
    ASSERT_TRUE(result.has_value());
    spdlog::shutdown();

    EXPECT_FALSE(std::filesystem::exists(expired_log));
    EXPECT_FALSE(std::filesystem::exists(excess_rotation));
    EXPECT_TRUE(std::filesystem::exists(unrelated_file));
}

TEST(ApplicationLoggingTest, FallsBackWithoutPreventingLogging) {
    auto options = Options(std::filesystem::path{});

    const auto result = InitializeApplicationLogging(options);

    EXPECT_FALSE(result.has_value());
    EXPECT_NO_THROW(SPDLOG_INFO("Terminal fallback remains available"));
}

} // namespace
} // namespace edit_atlas::support
