#ifndef EDIT_ATLAS_PRESENTATION_DESKTOP_INTEGRATION_HPP_
#define EDIT_ATLAS_PRESENTATION_DESKTOP_INTEGRATION_HPP_

#include <QString>
#include <QUrl>

namespace edit_atlas::presentation::desktop_integration {

/// Opens the containing directory and selects the file when supported.
[[nodiscard]] bool RevealFile(const QString &path);

/// Opens a local directory in the platform file manager.
[[nodiscard]] bool OpenDirectory(const QString &path);

/// Opens an HTTP or HTTPS URL using the platform default application.
[[nodiscard]] bool OpenExternalUrl(const QUrl &url);

} // namespace edit_atlas::presentation::desktop_integration

#endif // EDIT_ATLAS_PRESENTATION_DESKTOP_INTEGRATION_HPP_
