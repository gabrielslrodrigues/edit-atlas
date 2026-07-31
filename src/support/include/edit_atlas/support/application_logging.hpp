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

/// Base filename used by the active application log.
inline constexpr std::string_view kApplicationLogFilename = "edit-atlas.log";
/// Default rotation threshold of one mebibyte.
inline constexpr std::uintmax_t kDefaultMaximumLogFileSize = 1'048'576;
/// Default maximum number of active and rotated log files.
inline constexpr std::size_t kDefaultMaximumLogFiles = 5;
/// Default maximum retained log age of fourteen days.
inline constexpr std::chrono::hours kDefaultLogRetention =
    std::chrono::hours{24 * 14};

/// Controls persistent log rotation and age-based retention.
struct LoggingOptions final {
    /// Private directory in which logs are stored.
    std::filesystem::path directory;
    /// Maximum size of the active log before rotation.
    std::uintmax_t maximum_file_size;
    /// Maximum number of active and rotated log files.
    std::size_t maximum_files;
    /// Maximum age of a recognized application log.
    std::chrono::seconds maximum_age;
};

/// Describes a recoverable failure to initialize persistent logging.
struct LoggingInitializationFailure final {
    /// The directory that could not be prepared for logging.
    std::filesystem::path directory;
    /// A human-readable description of the initialization failure.
    std::string message;
};

/// Returns the private log directory below a platform application-data path.
///
/// \param application_data_path Platform-owned application-data root.
[[nodiscard]] std::filesystem::path
ApplicationLogDirectory(const std::filesystem::path &application_data_path);

/// Returns whether a filename belongs to Edit Atlas log rotation.
///
/// \param filename Basename to classify without accessing the filesystem.
[[nodiscard]] bool IsApplicationLogFilename(std::string_view filename) noexcept;

/// Configures terminal and rotating-file sinks as the default logger.
///
/// On failure, a terminal-only logger remains available and the caller may
/// continue launching the application.
///
/// \param options Rotation, retention, and destination policy.
/// \returns The active log path or a recoverable initialization failure.
[[nodiscard]] std::expected<std::filesystem::path, LoggingInitializationFailure>
InitializeApplicationLogging(const LoggingOptions &options);

} // namespace edit_atlas::support

#endif // EDIT_ATLAS_SUPPORT_APPLICATION_LOGGING_HPP_
