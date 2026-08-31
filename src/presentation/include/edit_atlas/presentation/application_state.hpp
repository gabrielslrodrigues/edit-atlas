#ifndef EDIT_ATLAS_PRESENTATION_APPLICATION_STATE_HPP_
#define EDIT_ATLAS_PRESENTATION_APPLICATION_STATE_HPP_

#include <filesystem>
#include <vector>

namespace edit_atlas::presentation {

/// Environment variable used by tests to isolate all persistent state.
inline constexpr auto kTestStateRootEnvironment = "EDIT_ATLAS_TEST_STATE_ROOT";

/// Configures persistent settings before their first use.
///
/// When `EDIT_ATLAS_TEST_STATE_ROOT` is nonempty, default QSettings instances
/// use INI files beneath that directory. Production behavior is unchanged when
/// the variable is absent.
void ConfigureApplicationState(void);

/// Returns the root used for application-local persistent data.
///
/// The test override is returned directly when configured. Otherwise this is
/// the platform application-local data directory selected by Qt.
[[nodiscard]] std::filesystem::path ConfiguredApplicationDataDirectory(void);

/// Returns the directory containing persisted timeline templates.
[[nodiscard]] std::filesystem::path ConfiguredTemplateDirectory(void);

/// Returns whether successfully opened files should be remembered.
[[nodiscard]] bool RememberRecentFilesEnabled(void);

/// Enables or disables recent-file persistence.
///
/// Disabling the preference also removes the stored file history.
void SetRememberRecentFilesEnabled(bool enabled);

/// Returns recently opened files in most-recent-first order.
[[nodiscard]] std::vector<std::filesystem::path> ConfiguredRecentFiles(void);

/// Moves a successfully opened file to the front of the recent-file list.
///
/// The request is ignored while recent-file persistence is disabled.
void RecordRecentFile(const std::filesystem::path &path);

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_APPLICATION_STATE_HPP_
