#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_STYLE_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_STYLE_HPP_

#include <edit_atlas/presentation/appearance.hpp>

#include <QPalette>
#include <QString>

class QApplication;

namespace edit_atlas::frontends::widgets {

/// Loads the embedded stylesheet with the palette's colors substituted.
///
/// The stylesheet is a template: it names shared appearance tokens rather
/// than colors, so light and dark differ only by the table applied to it.
/// Returns an empty string when the resource cannot be read, and leaves an
/// unrecognized token in place so it is visible rather than silently blank.
[[nodiscard]] QString
LoadApplicationStyleSheet(const presentation::AppearancePalette &palette);

/// Builds the widget palette for one shared appearance palette.
[[nodiscard]] QPalette
BuildApplicationPalette(const presentation::AppearancePalette &palette);

/// Applies a palette and its stylesheet to a running application.
///
/// Safe to call repeatedly: it is how an appearance change is applied without
/// restarting.
void ApplyApplicationAppearance(
    QApplication &application,
    const presentation::AppearancePalette &palette);

/// Applies the compact application behavior, then the given appearance.
void ApplyApplicationStyle(QApplication &application,
                           const presentation::AppearancePalette &palette);

} // namespace edit_atlas::frontends::widgets

#endif // EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_STYLE_HPP_
