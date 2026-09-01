#include <edit_atlas/frontends/quick/style/appearance_style.hpp>

#include <edit_atlas/presentation/appearance.hpp>

#include <QObject>
#include <QString>

namespace edit_atlas::frontends::quick::style {

AppearanceStyle::AppearanceStyle(QObject *parent) : QObject{parent} {
    connect(&controller_,
            &presentation::AppearanceController::appearanceChanged, this,
            &AppearanceStyle::appearanceChanged);
    connect(&controller_,
            &presentation::AppearanceController::resolvedAppearanceChanged,
            this, &AppearanceStyle::paletteChanged);
}

QString AppearanceStyle::AppearanceCode(void) const {
    return presentation::AppearanceCode(controller_.Appearance());
}

void AppearanceStyle::SetAppearanceCode(const QString &code) {
    controller_.SetAppearance(presentation::AppearanceFromCode(code));
}

presentation::AppearancePalette AppearanceStyle::Palette(void) const {
    return controller_.Palette();
}

} // namespace edit_atlas::frontends::quick::style
