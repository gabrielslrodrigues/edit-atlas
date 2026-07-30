#ifndef EDIT_ATLAS_APP_DIAGNOSTIC_SUPPORT_HPP_
#define EDIT_ATLAS_APP_DIAGNOSTIC_SUPPORT_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <filesystem>

namespace edit_atlas::app {

/// Returns the platform-appropriate private application log directory.
[[nodiscard]] std::filesystem::path ConfiguredLogDirectory(void);

/// Collects the fixed, non-sensitive runtime metadata used for diagnostics.
[[nodiscard]] support::DiagnosticEnvironment
CreateDiagnosticEnvironment(const core::FormatRegistry &registry);

/// Writes the diagnostic environment to the configured application logger.
void LogDiagnosticEnvironment(
    const support::DiagnosticEnvironment &environment);

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_DIAGNOSTIC_SUPPORT_HPP_
