// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "edit_atlas/presentation/appearance.hpp"

#include "QCoreApplication"
#include "QGuiApplication"
#include "QSettings"
#include "QString"
#include "QStyleHints"
#include "Qt"

namespace edit_atlas::presentation {
namespace {

constexpr auto kAppearanceKey = "interface/appearance";
constexpr auto kSystemValue = "system";
constexpr auto kLightValue = "light";
constexpr auto kDarkValue = "dark";

ApplicationAppearance ConfiguredApplicationAppearance(void) {
  const QSettings settings;
  return AppearanceFromCode(settings
                                .value(QString::fromLatin1(kAppearanceKey),
                                       QString::fromLatin1(kSystemValue))
                                .toString());
}

void SaveApplicationAppearance(ApplicationAppearance appearance) {
  QSettings settings;
  settings.setValue(QString::fromLatin1(kAppearanceKey),
                    AppearanceCode(appearance));
}

bool SystemPrefersDarkAppearance(void) {
  // Console tools and unit tests run without a GUI application, where no
  // platform theme exists to ask. Light is the documented Qt fallback.
  if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
    return false;
  }
  const auto* hints = QGuiApplication::styleHints();
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
  if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
    return;
  }
  auto* hints = QGuiApplication::styleHints();
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

}  // namespace

QString AppearanceCode(ApplicationAppearance appearance) {
  switch (appearance) {
    case ApplicationAppearance::kLight:
      return QString::fromLatin1(kLightValue);
    case ApplicationAppearance::kDark:
      return QString::fromLatin1(kDarkValue);
    case ApplicationAppearance::kSystem:
      break;
  }
  return QString::fromLatin1(kSystemValue);
}

ApplicationAppearance AppearanceFromCode(const QString& code) {
  if (code == QString::fromLatin1(kLightValue)) {
    return ApplicationAppearance::kLight;
  }
  if (code == QString::fromLatin1(kDarkValue)) {
    return ApplicationAppearance::kDark;
  }
  return ApplicationAppearance::kSystem;
}

const AppearancePalette& AppearancePaletteFor(ResolvedAppearance appearance) {
  // Both frontends may retain references through process teardown.
  static const auto& kDarkPalette = *new const AppearancePalette{
      .accent = QStringLiteral("#7aa2ff"),
      .accent_hovered = QStringLiteral("#91b2ff"),
      .accent_pressed = QStringLiteral("#638ce8"),
      .on_accent = QStringLiteral("#101319"),
      .window = QStringLiteral("#14171c"),
      .surface = QStringLiteral("#1d2128"),
      .surface_alternate = QStringLiteral("#20232a"),
      .control = QStringLiteral("#262b33"),
      .control_hovered = QStringLiteral("#303741"),
      .control_pressed = QStringLiteral("#1f242b"),
      .border = QStringLiteral("#3b424d"),
      .focus = QStringLiteral("#90b4ff"),
      .disabled = QStringLiteral("#555c66"),
      .text_primary = QStringLiteral("#f0f2f5"),
      .text_secondary = QStringLiteral("#aeb5bf"),
      .text_inverted = QStringLiteral("#14171c"),
      .warning = QStringLiteral("#f6c177"),
      .danger = QStringLiteral("#ff8a80"),
      .tooltip_surface = QStringLiteral("#262930"),
      .tooltip_text = QStringLiteral("#f2f3f5"),
  };

  static const auto& kLightPalette = *new const AppearancePalette{
      .accent = QStringLiteral("#315fcb"),
      .accent_hovered = QStringLiteral("#254fae"),
      .accent_pressed = QStringLiteral("#1d4193"),
      .on_accent = QStringLiteral("#ffffff"),
      .window = QStringLiteral("#f4f5f7"),
      .surface = QStringLiteral("#ffffff"),
      .surface_alternate = QStringLiteral("#eef0f4"),
      .control = QStringLiteral("#ffffff"),
      .control_hovered = QStringLiteral("#f1f3f6"),
      .control_pressed = QStringLiteral("#e4e7ec"),
      .border = QStringLiteral("#c9ced6"),
      .focus = QStringLiteral("#315fcb"),
      .disabled = QStringLiteral("#9aa1ac"),
      .text_primary = QStringLiteral("#20242b"),
      .text_secondary = QStringLiteral("#626a75"),
      .text_inverted = QStringLiteral("#ffffff"),
      .warning = QStringLiteral("#9a6212"),
      .danger = QStringLiteral("#b3261e"),
      .tooltip_surface = QStringLiteral("#20242b"),
      .tooltip_text = QStringLiteral("#f4f5f7"),
  };
  switch (appearance) {
    case ResolvedAppearance::kLight:
      return kLightPalette;
    case ResolvedAppearance::kDark:
      break;
  }
  return kDarkPalette;
}

AppearanceController::AppearanceController(QObject* parent)
    : QObject{parent},
      appearance_{ConfiguredApplicationAppearance()},
      resolved_{ResolveAppearance(appearance_)} {
  if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
    return;
  }
  if (auto* hints = QGuiApplication::styleHints(); hints != nullptr) {
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

const AppearancePalette& AppearanceController::Palette(void) const {
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

}  // namespace edit_atlas::presentation
