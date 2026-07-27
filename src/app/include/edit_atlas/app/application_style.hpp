#ifndef EDIT_ATLAS_APP_APPLICATION_STYLE_HPP_
#define EDIT_ATLAS_APP_APPLICATION_STYLE_HPP_

class QApplication;

namespace edit_atlas::app {

/// Applies the compact Edit Atlas palette and disables costly UI animations.
void ApplyApplicationStyle(QApplication &application);

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_APPLICATION_STYLE_HPP_
