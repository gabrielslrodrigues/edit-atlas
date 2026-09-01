#include <edit_atlas/frontends/quick/style/appearance_style.hpp>

#include <edit_atlas/presentation/appearance.hpp>

#include <QObject>

namespace edit_atlas::frontends::quick::style {

AppearanceStyle::AppearanceStyle(QObject *parent) : QObject{parent} {
    connect(&controller_,
            &presentation::AppearanceController::appearanceChanged, this,
            &AppearanceStyle::appearanceChanged);
    connect(&controller_,
            &presentation::AppearanceController::resolvedAppearanceChanged,
            this, &AppearanceStyle::paletteChanged);
}

presentation::ApplicationAppearance
AppearanceStyle::Appearance(void) const {
    return controller_.Appearance();
}

void AppearanceStyle::SetAppearance(
    presentation::ApplicationAppearance appearance) {
    controller_.SetAppearance(appearance);
}

presentation::AppearancePalette AppearanceStyle::Palette(void) const {
    return controller_.Palette();
}

} // namespace edit_atlas::frontends::quick::style
