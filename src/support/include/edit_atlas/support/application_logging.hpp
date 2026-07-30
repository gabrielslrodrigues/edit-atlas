#ifndef EDIT_ATLAS_SUPPORT_APPLICATION_LOGGING_HPP_
#define EDIT_ATLAS_SUPPORT_APPLICATION_LOGGING_HPP_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace edit_atlas::support {

inline constexpr std::string_view kApplicationLogFilename = "edit-atlas.log";
inline constexpr std::uintmax_t kDefaultMaximumLogFileSize = 1'048'576;
inline constexpr std::size_t kDefaultMaximumLogFiles = 5;
inline constexpr std::chrono::hours kDefaultLogRetention =
    std::chrono::hours{24 * 14};

/// Controls persistent log rotation and age-based retention.
struct LoggingOptions final {
    std::filesystem::path directory;
    std::uintmax_t maximum_file_size;
    std::size_t maximum_files;
    std::chrono::seconds maximum_age;
};

/// Describes a recoverable failure to initialize persistent logging.
struct LoggingInitializationFailure final {
    std::filesystem::path directory;
    std::string message;
};

/// Returns the private log directory below a platform application-data path.
[[nodiscard]] std::filesystem::path
ApplicationLogDirectory(const std::filesystem::path &application_data_path);

/// Returns whether a filename belongs to Edit Atlas log rotation.
[[nodiscard]] bool IsApplicationLogFilename(std::string_view filename) noexcept;

/// Configures terminal and rotating-file sinks as the default logger.
///
/// On failure, a terminal-only logger remains available and the caller may
/// continue launching the application.
[[nodiscard]] std::expected<std::filesystem::path, LoggingInitializationFailure>
InitializeApplicationLogging(const LoggingOptions &options);

} // namespace edit_atlas::support

#endif // EDIT_ATLAS_SUPPORT_APPLICATION_LOGGING_HPP_
