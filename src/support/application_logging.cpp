#include <edit_atlas/support/application_logging.hpp>

#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace edit_atlas::support {
namespace {

constexpr std::string_view kLoggerName = "edit-atlas";
constexpr std::string_view kLogPattern =
    "[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v";

[[nodiscard]] std::string Utf8Filename(const std::filesystem::path &path) {
    const auto utf8 = path.filename().u8string();
    return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

[[nodiscard]] std::optional<std::size_t>
RotationIndex(std::string_view filename) noexcept {
    if (!IsApplicationLogFilename(filename) ||
        filename == kApplicationLogFilename) {
        return std::nullopt;
    }
    constexpr std::string_view kPrefix = "edit-atlas.";
    constexpr std::string_view kSuffix = ".log";
    const auto index_text = filename.substr(
        kPrefix.size(), filename.size() - kPrefix.size() - kSuffix.size());
    std::size_t index = 0;
    const auto result = std::from_chars(
        index_text.data(), index_text.data() + index_text.size(), index);
    if (result.ec != std::errc{} ||
        result.ptr != index_text.data() + index_text.size()) {
        return std::nullopt;
    }
    return index;
}

class FilesystemRotatingSink final
    : public spdlog::sinks::base_sink<std::mutex> {
  public:
    FilesystemRotatingSink(std::filesystem::path path,
                           std::size_t maximum_file_size,
                           std::size_t maximum_files)
        : path_(std::move(path)), maximum_file_size_(maximum_file_size),
          maximum_files_(maximum_files) {
        Open(false);
        if (current_size_ > maximum_file_size_) {
            Rotate();
        }
    }

    ~FilesystemRotatingSink(void) override = default;

    FilesystemRotatingSink(const FilesystemRotatingSink &) = delete;
    FilesystemRotatingSink &operator=(const FilesystemRotatingSink &) = delete;
    FilesystemRotatingSink(FilesystemRotatingSink &&) = delete;
    FilesystemRotatingSink &operator=(FilesystemRotatingSink &&) = delete;

  protected:
    void sink_it_(const spdlog::details::log_msg &message) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(message, formatted);
        const auto count = (std::min)(formatted.size(), maximum_file_size_);
        if (current_size_ != 0 && current_size_ + count > maximum_file_size_) {
            Rotate();
        }

        file_.write(formatted.data(), static_cast<std::streamsize>(count));
        if (!file_) {
            throw spdlog::spdlog_ex{"Could not write the application log."};
        }
        current_size_ += count;
    }

    void flush_(void) override {
        file_.flush();
        if (!file_) {
            throw spdlog::spdlog_ex{"Could not flush the application log."};
        }
    }

  private:
    [[nodiscard]] std::filesystem::path RotatedPath(std::size_t index) const {
        return path_.parent_path() /
               ("edit-atlas." + std::to_string(index) + ".log");
    }

    void Open(bool truncate) {
        const auto mode = std::ios::binary | std::ios::out |
                          (truncate ? std::ios::trunc : std::ios::app);
        file_.open(path_, mode);
        if (!file_.is_open()) {
            throw spdlog::spdlog_ex{"Could not open the application log."};
        }

        if (truncate) {
            current_size_ = 0;
            return;
        }
        std::error_code error;
        current_size_ = std::filesystem::file_size(path_, error);
        if (error) {
            throw spdlog::spdlog_ex{"Could not inspect the application log."};
        }
    }

    void Rotate(void) {
        file_.close();
        std::error_code error;
        static_cast<void>(
            std::filesystem::remove(RotatedPath(maximum_files_ - 1), error));
        if (error) {
            throw spdlog::spdlog_ex{
                "Could not remove an expired log rotation."};
        }

        for (auto index = maximum_files_ - 1; index > 1; --index) {
            const auto source = RotatedPath(index - 1);
            if (!std::filesystem::exists(source, error)) {
                if (error) {
                    throw spdlog::spdlog_ex{
                        "Could not inspect a log rotation."};
                }
                continue;
            }
            std::filesystem::rename(source, RotatedPath(index), error);
            if (error) {
                throw spdlog::spdlog_ex{"Could not rotate an application log."};
            }
        }

        if (std::filesystem::exists(path_, error)) {
            if (error) {
                throw spdlog::spdlog_ex{
                    "Could not inspect the active application log."};
            }
            std::filesystem::rename(path_, RotatedPath(1), error);
            if (error) {
                throw spdlog::spdlog_ex{
                    "Could not rotate the active application log."};
            }
        } else if (error) {
            throw spdlog::spdlog_ex{
                "Could not inspect the active application log."};
        }
        Open(true);
    }

    std::filesystem::path path_;
    std::ofstream file_;
    std::size_t maximum_file_size_;
    std::size_t maximum_files_;
    std::uintmax_t current_size_ = 0;
};

void ConfigureDefaultLogger(std::vector<spdlog::sink_ptr> sinks) {
    spdlog::drop(std::string{kLoggerName});
    auto logger = std::make_shared<spdlog::logger>(std::string{kLoggerName},
                                                   sinks.begin(), sinks.end());
    logger->set_pattern(std::string{kLogPattern});
    logger->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(logger));
}

void ConfigureTerminalLogger(void) {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    ConfigureDefaultLogger(std::move(sinks));
}

[[nodiscard]] std::expected<void, std::string>
PruneExpiredLogs(const LoggingOptions &options) {
    const auto cutoff =
        std::filesystem::file_time_type::clock::now() - options.maximum_age;
    std::error_code error;
    std::filesystem::directory_iterator files{options.directory, error};
    if (error) {
        return std::unexpected(error.message());
    }

    for (const auto &entry : files) {
        const auto status = entry.symlink_status(error);
        if (error) {
            return std::unexpected(error.message());
        }
        const auto filename = Utf8Filename(entry.path());
        if (!std::filesystem::is_regular_file(status) ||
            !IsApplicationLogFilename(filename)) {
            continue;
        }

        const auto modified = entry.last_write_time(error);
        if (error) {
            return std::unexpected(error.message());
        }
        const auto rotation = RotationIndex(filename);
        const auto exceeds_file_limit =
            rotation.has_value() && *rotation >= options.maximum_files;
        if (modified < cutoff || exceeds_file_limit) {
            static_cast<void>(std::filesystem::remove(entry.path(), error));
            if (error) {
                return std::unexpected(error.message());
            }
        }
    }
    return {};
}

[[nodiscard]] LoggingInitializationFailure
Failure(const LoggingOptions &options, std::string message) {
    return LoggingInitializationFailure{
        .directory = options.directory,
        .message = std::move(message),
    };
}

} // namespace

std::filesystem::path
ApplicationLogDirectory(const std::filesystem::path &application_data_path) {
    if (application_data_path.empty()) {
        return {};
    }
    return application_data_path / "logs";
}

bool IsApplicationLogFilename(std::string_view filename) noexcept {
    if (filename == kApplicationLogFilename) {
        return true;
    }

    constexpr std::string_view kPrefix = "edit-atlas.";
    constexpr std::string_view kSuffix = ".log";
    if (!filename.starts_with(kPrefix) || !filename.ends_with(kSuffix)) {
        return false;
    }
    const auto index = filename.substr(
        kPrefix.size(), filename.size() - kPrefix.size() - kSuffix.size());
    return !index.empty() && index != "0" &&
           (index.size() == 1 || index.front() != '0') &&
           std::ranges::all_of(index, [](char character) {
               return character >= '0' && character <= '9';
           });
}

std::expected<std::filesystem::path, LoggingInitializationFailure>
InitializeApplicationLogging(const LoggingOptions &options) {
    try {
        if (options.directory.empty()) {
            ConfigureTerminalLogger();
            return std::unexpected(
                Failure(options, "The log directory is empty."));
        }
        if (options.maximum_file_size == 0 ||
            options.maximum_file_size >
                (std::numeric_limits<std::size_t>::max)() ||
            options.maximum_file_size >
                static_cast<std::uintmax_t>(
                    (std::numeric_limits<std::streamsize>::max)()) ||
            options.maximum_files < 2 ||
            options.maximum_age <= std::chrono::seconds::zero()) {
            ConfigureTerminalLogger();
            return std::unexpected(
                Failure(options, "The log rotation limits are invalid."));
        }

        std::error_code error;
        std::filesystem::create_directories(options.directory, error);
        if (error) {
            ConfigureTerminalLogger();
            return std::unexpected(Failure(options, error.message()));
        }
#if !defined(_WIN32)
        std::filesystem::permissions(
            options.directory, std::filesystem::perms::owner_all,
            std::filesystem::perm_options::replace, error);
        if (error) {
            ConfigureTerminalLogger();
            return std::unexpected(Failure(options, error.message()));
        }
#endif
        if (const auto prune_result = PruneExpiredLogs(options);
            !prune_result.has_value()) {
            ConfigureTerminalLogger();
            return std::unexpected(Failure(options, prune_result.error()));
        }

        const auto log_path =
            options.directory / std::string{kApplicationLogFilename};
        std::vector<spdlog::sink_ptr> sinks;
        sinks.emplace_back(
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        sinks.emplace_back(std::make_shared<FilesystemRotatingSink>(
            log_path, static_cast<std::size_t>(options.maximum_file_size),
            options.maximum_files));
        ConfigureDefaultLogger(std::move(sinks));
        return log_path;
    } catch (const std::exception &exception) {
        try {
            ConfigureTerminalLogger();
        } catch (...) {
        }
        return std::unexpected(Failure(options, exception.what()));
    } catch (...) {
        try {
            ConfigureTerminalLogger();
        } catch (...) {
        }
        return std::unexpected(
            Failure(options, "Unknown logging initialization failure."));
    }
}

} // namespace edit_atlas::support
