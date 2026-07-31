#ifndef EDIT_ATLAS_APP_DESKTOP_INTEGRATION_HPP_
#define EDIT_ATLAS_APP_DESKTOP_INTEGRATION_HPP_

#include <QString>

namespace edit_atlas::app::desktop_integration {

/// Opens the containing directory and selects the file when supported.
[[nodiscard]] bool RevealFile(const QString &path);

} // namespace edit_atlas::app::desktop_integration

#endif // EDIT_ATLAS_APP_DESKTOP_INTEGRATION_HPP_
