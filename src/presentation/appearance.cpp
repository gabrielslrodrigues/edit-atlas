#include <edit_atlas/presentation/appearance.hpp>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSettings>
#include <QString>
#include <QStyleHints>
#include <Qt>

namespace edit_atlas::presentation {
namespace {

constexpr auto kAppearanceKey = "interface/appearance";
constexpr auto kSystemValue = "system";
constexpr auto kLightValue = "light";
constexpr auto kDarkValue = "dark";

// Both frontends read these. The dark values are the ones the Qt Quick
// frontend shipped, so the production appearance is unchanged by unifying the
// tables; the light values are its light theme, extended with the semantic
// colors the Qt Widgets stylesheet also needs.
const AppearancePalette kDarkPalette{
    .accent = QStringLiteral("#7aa2ff"),
    .accentHovered = QStringLiteral("#91b2ff"),
    .accentPressed = QStringLiteral("#638ce8"),
    .onAccent = QStringLiteral("#101319"),
    .window = QStringLiteral("#14171c"),
    .surface = QStringLiteral("#1d2128"),
    .surfaceAlternate = QStringLiteral("#20232a"),
    .control = QStringLiteral("#262b33"),
    .controlHovered = QStringLiteral("#303741"),
    .controlPressed = QStringLiteral("#1f242b"),
    .border = QStringLiteral("#3b424d"),
    .focus = QStringLiteral("#90b4ff"),
    .disabled = QStringLiteral("#555c66"),
    .textPrimary = QStringLiteral("#f0f2f5"),
    .textSecondary = QStringLiteral("#aeb5bf"),
    .textInverted = QStringLiteral("#14171c"),
    .warning = QStringLiteral("#f6c177"),
    .danger = QStringLiteral("#ff8a80"),
    .tooltipSurface = QStringLiteral("#262930"),
    .tooltipText = QStringLiteral("#f2f3f5"),
};

const AppearancePalette kLightPalette{
    .accent = QStringLiteral("#315fcb"),
    .accentHovered = QStringLiteral("#254fae"),
    .accentPressed = QStringLiteral("#1d4193"),
    .onAccent = QStringLiteral("#ffffff"),
    .window = QStringLiteral("#f4f5f7"),
    .surface = QStringLiteral("#ffffff"),
    .surfaceAlternate = QStringLiteral("#eef0f4"),
    .control = QStringLiteral("#ffffff"),
    .controlHovered = QStringLiteral("#f1f3f6"),
    .controlPressed = QStringLiteral("#e4e7ec"),
    .border = QStringLiteral("#c9ced6"),
    .focus = QStringLiteral("#315fcb"),
    .disabled = QStringLiteral("#9aa1ac"),
    .textPrimary = QStringLiteral("#20242b"),
    .textSecondary = QStringLiteral("#626a75"),
    .textInverted = QStringLiteral("#ffffff"),
    .warning = QStringLiteral("#9a6212"),
    .danger = QStringLiteral("#b3261e"),
    .tooltipSurface = QStringLiteral("#20242b"),
    .tooltipText = QStringLiteral("#f4f5f7"),
};

ApplicationAppearance ConfiguredApplicationAppearance(void) {
    const QSettings settings;
    const auto value =
        settings
            .value(QString::fromLatin1(kAppearanceKey),
                   QString::fromLatin1(kSystemValue))
            .toString();
    if (value == QString::fromLatin1(kLightValue)) {
        return ApplicationAppearance::kLight;
    }
    if (value == QString::fromLatin1(kDarkValue)) {
        return ApplicationAppearance::kDark;
    }
    return ApplicationAppearance::kSystem;
}

void SaveApplicationAppearance(ApplicationAppearance appearance) {
    QSettings settings;
    switch (appearance) {
    case ApplicationAppearance::kLight:
        settings.setValue(QString::fromLatin1(kAppearanceKey),
                          QString::fromLatin1(kLightValue));
        return;
    case ApplicationAppearance::kDark:
        settings.setValue(QString::fromLatin1(kAppearanceKey),
                          QString::fromLatin1(kDarkValue));
        return;
    case ApplicationAppearance::kSystem:
        settings.setValue(QString::fromLatin1(kAppearanceKey),
                          QString::fromLatin1(kSystemValue));
        return;
    }
}

bool SystemPrefersDarkAppearance(void) {
    // Console tools and unit tests run without a GUI application, where no
    // platform theme exists to ask. Light is the documented Qt fallback.
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()) ==
        nullptr) {
        return false;
    }
    const auto *hints = QGuiApplication::styleHints();
    if (hints == nullptr) {
        return false;
    }
    return hints->colorScheme() == Qt::ColorScheme::Dark;
}

// Native window decorations, dialogs, and menus follow the platform color
// scheme, so a selection is requested from the platform rather than painted
// over. Following the system is expressed by holding no override, which is
// what `Unknown` means to Qt.
void RequestPlatformColorScheme(ApplicationAppearance appearance) {
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()) ==
        nullptr) {
        return;
    }
    auto *hints = QGuiApplication::styleHints();
    if (hints == nullptr) {
        return;
    }
    switch (appearance) {
    case ApplicationAppearance::kLight:
        hints->setColorScheme(Qt::ColorScheme::Light);
        return;
    case ApplicationAppearance::kDark:
        hints->setColorScheme(Qt::ColorScheme::Dark);
        return;
    case ApplicationAppearance::kSystem:
        hints->setColorScheme(Qt::ColorScheme::Unknown);
        return;
    }
}

ResolvedAppearance ResolveAppearance(ApplicationAppearance appearance) {
    switch (appearance) {
    case ApplicationAppearance::kLight:
        return ResolvedAppearance::kLight;
    case ApplicationAppearance::kDark:
        return ResolvedAppearance::kDark;
    case ApplicationAppearance::kSystem:
        break;
    }
    return SystemPrefersDarkAppearance() ? ResolvedAppearance::kDark
                                         : ResolvedAppearance::kLight;
}

} // namespace

const AppearancePalette &AppearancePaletteFor(ResolvedAppearance appearance) {
    switch (appearance) {
    case ResolvedAppearance::kLight:
        return kLightPalette;
    case ResolvedAppearance::kDark:
        break;
    }
    return kDarkPalette;
}

AppearanceController::AppearanceController(QObject *parent)
    : QObject{parent}, appearance_{ConfiguredApplicationAppearance()},
      resolved_{ResolveAppearance(appearance_)} {
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()) ==
        nullptr) {
        return;
    }
    if (auto *hints = QGuiApplication::styleHints(); hints != nullptr) {
        connect(hints, &QStyleHints::colorSchemeChanged, this,
                [this](Qt::ColorScheme) { RefreshResolvedAppearance(); });
    }
    RequestPlatformColorScheme(appearance_);
}

ApplicationAppearance AppearanceController::Appearance(void) const {
    return appearance_;
}

void AppearanceController::SetAppearance(ApplicationAppearance appearance) {
    if (appearance_ == appearance) {
        return;
    }
    appearance_ = appearance;
    SaveApplicationAppearance(appearance_);
    emit appearanceChanged();
    RequestPlatformColorScheme(appearance_);
    RefreshResolvedAppearance();
}

ResolvedAppearance AppearanceController::ResolvedAppearanceValue(void) const {
    return resolved_;
}

const AppearancePalette &AppearanceController::Palette(void) const {
    return AppearancePaletteFor(resolved_);
}

void AppearanceController::RefreshResolvedAppearance(void) {
    const auto resolved = ResolveAppearance(appearance_);
    if (resolved == resolved_) {
        return;
    }
    resolved_ = resolved;
    emit resolvedAppearanceChanged();
}

} // namespace edit_atlas::presentation
