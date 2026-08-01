#ifndef EDIT_ATLAS_CLI_APPLICATION_HPP_
#define EDIT_ATLAS_CLI_APPLICATION_HPP_

#include <iosfwd>
#include <span>
#include <string_view>

namespace edit_atlas::cli {

/// Stable process outcomes exposed by the command-line frontend.
enum class ExitCode : int {
    /// The command completed without diagnostics requiring attention.
    kSuccess = 0,
    /// The command completed but produced one or more warnings.
    kWarnings = 1,
    /// The command line is incomplete or invalid.
    kUsageError = 2,
    /// The input was readable but could not be imported.
    kInvalidInput = 3,
    /// The destination exists and replacement was not authorized.
    kDestinationExists = 4,
    /// A filesystem or application operation failed.
    kOperationalFailure = 5,
};

/// Runs the command-line frontend using UTF-8 arguments.
///
/// `arguments` excludes the executable name. Completion information is written
/// to `output`; diagnostics and failures are written to `error`.
[[nodiscard]] ExitCode Run(std::span<const std::string_view> arguments,
                           std::ostream &output, std::ostream &error);

} // namespace edit_atlas::cli

#endif // EDIT_ATLAS_CLI_APPLICATION_HPP_
