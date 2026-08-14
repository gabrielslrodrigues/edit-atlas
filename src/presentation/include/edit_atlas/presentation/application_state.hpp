#ifndef EDIT_ATLAS_PRESENTATION_APPLICATION_STATE_HPP_
#define EDIT_ATLAS_PRESENTATION_APPLICATION_STATE_HPP_

#include <filesystem>

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

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_APPLICATION_STATE_HPP_
