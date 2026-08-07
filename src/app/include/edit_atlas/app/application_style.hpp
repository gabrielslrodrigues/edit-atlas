#ifndef EDIT_ATLAS_APP_APPLICATION_STYLE_HPP_
#define EDIT_ATLAS_APP_APPLICATION_STYLE_HPP_

#include <QString>

class QApplication;

namespace edit_atlas::app {

/// Loads the embedded visual stylesheet, or returns an empty string on failure.
[[nodiscard]] QString LoadApplicationStyleSheet(void);

/// Applies the compact application behavior, palette, and visual stylesheet.
void ApplyApplicationStyle(QApplication &application);

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_APPLICATION_STYLE_HPP_
