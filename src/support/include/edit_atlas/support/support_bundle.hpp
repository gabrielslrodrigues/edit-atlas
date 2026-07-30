#ifndef EDIT_ATLAS_SUPPORT_SUPPORT_BUNDLE_HPP_
#define EDIT_ATLAS_SUPPORT_SUPPORT_BUNDLE_HPP_

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace edit_atlas::support {

/// Non-sensitive runtime metadata included in diagnostic support bundles.
struct DiagnosticEnvironment final {
    std::string application_version;
    std::string operating_system;
    std::string architecture;
    std::string qt_version;
    std::string platform_plugin;
    std::vector<std::string> importer_formats;
    std::vector<std::string> exporter_formats;
};

/// Describes one local support-bundle export.
struct SupportBundleRequest final {
    std::filesystem::path path;
    std::filesystem::path log_directory;
    DiagnosticEnvironment environment;
    bool replace_existing;
};

/// Identifies the stage at which support-bundle creation failed.
enum class SupportBundleFailureKind {
    kDestinationExists,
    kReadLogsFailed,
    kWriteBundleFailed,
    kCommitFailed,
};

/// Contains the path and file count from a completed support bundle.
struct SupportBundleReceipt final {
    std::filesystem::path path;
    std::size_t log_file_count;
};

/// Contains a presentation-neutral support-bundle failure.
struct SupportBundleFailure final {
    std::filesystem::path path;
    SupportBundleFailureKind kind;
    std::error_code filesystem_error;
    std::string detail;
};

using CreateSupportBundleResult =
    std::expected<SupportBundleReceipt, SupportBundleFailure>;

/// Produces the human-readable, privacy-limited environment summary.
[[nodiscard]] std::string
FormatDiagnosticEnvironment(const DiagnosticEnvironment &environment);

/// Creates a ZIP containing `environment.txt` and recognized application logs.
///
/// No user documents, environment variables, or files with unrecognized names
/// are read or included.
[[nodiscard]] CreateSupportBundleResult
CreateSupportBundle(SupportBundleRequest request);

} // namespace edit_atlas::support

#endif // EDIT_ATLAS_SUPPORT_SUPPORT_BUNDLE_HPP_
