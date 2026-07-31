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
    /// The Edit Atlas application version.
    std::string application_version;
    /// The operating-system name and release reported by Qt.
    std::string operating_system;
    /// The runtime CPU architecture reported by Qt.
    std::string architecture;
    /// The Qt runtime version.
    std::string qt_version;
    /// The active Qt platform plugin.
    std::string platform_plugin;
    /// Stable identifiers of registered import formats.
    std::vector<std::string> importer_formats;
    /// Stable identifiers of registered export formats.
    std::vector<std::string> exporter_formats;
};

/// Describes one local support-bundle export.
struct SupportBundleRequest final {
    /// The requested ZIP destination.
    std::filesystem::path path;
    /// The private directory containing recognized application logs.
    std::filesystem::path log_directory;
    /// The explicit non-sensitive environment metadata to include.
    DiagnosticEnvironment environment;
    /// Whether an existing destination may be atomically replaced.
    bool replace_existing;
};

/// Identifies the stage at which support-bundle creation failed.
enum class SupportBundleFailureKind {
    /// The destination exists and replacement was not authorized.
    kDestinationExists,
    /// A recognized application log could not be read.
    kReadLogsFailed,
    /// The bundle could not be written to a temporary sibling file.
    kWriteBundleFailed,
    /// The completed temporary file could not replace the destination.
    kCommitFailed,
};

/// Contains the path and file count from a completed support bundle.
struct SupportBundleReceipt final {
    /// The committed bundle path.
    std::filesystem::path path;
    /// The number of recognized log files included in the bundle.
    std::size_t log_file_count;
};

/// Contains a presentation-neutral support-bundle failure.
struct SupportBundleFailure final {
    /// The requested bundle destination.
    std::filesystem::path path;
    /// The stage at which bundle creation failed.
    SupportBundleFailureKind kind;
    /// The native filesystem error, when one is available.
    std::error_code filesystem_error;
    /// Additional presentation-neutral failure context.
    std::string detail;
};

/// Either a completed support-bundle receipt or a stage-specific failure.
using CreateSupportBundleResult =
    std::expected<SupportBundleReceipt, SupportBundleFailure>;

/// Produces the human-readable, privacy-limited environment summary.
///
/// \param environment Explicit metadata to format.
[[nodiscard]] std::string
FormatDiagnosticEnvironment(const DiagnosticEnvironment &environment);

/// Creates a ZIP containing `environment.txt` and recognized application logs.
///
/// No user documents, environment variables, or files with unrecognized names
/// are read or included.
///
/// \param request Destination, log directory, and explicit environment data.
/// \returns A receipt on success or a stage-specific failure.
[[nodiscard]] CreateSupportBundleResult
CreateSupportBundle(SupportBundleRequest request);

} // namespace edit_atlas::support

#endif // EDIT_ATLAS_SUPPORT_SUPPORT_BUNDLE_HPP_
