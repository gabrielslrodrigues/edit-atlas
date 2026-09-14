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

#ifndef EDIT_ATLAS_PRESENTATION_APPEARANCE_HPP_
#define EDIT_ATLAS_PRESENTATION_APPEARANCE_HPP_

#include "QObject"
#include "QString"

namespace edit_atlas::presentation {

Q_NAMESPACE

/// An interface appearance a user can select.
enum class ApplicationAppearance {
  /// Follow the platform color scheme, and keep following it as it changes.
  kSystem,
  /// Always present the light palette.
  kLight,
  /// Always present the dark palette.
  kDark,
};
Q_ENUM_NS(ApplicationAppearance)

/// An appearance the application actually presents.
///
/// `ApplicationAppearance::kSystem` is a selection rather than a presentation,
/// so it has no member here. Adding a palette, such as a high-contrast one,
/// means adding a value here rather than reinterpreting a flag.
enum class ResolvedAppearance {
  /// Light palette.
  kLight,
  /// Dark palette.
  kDark,
};
Q_ENUM_NS(ResolvedAppearance)

/// Named colors of one resolved appearance.
///
/// Values are `#rrggbb` strings rather than `QColor` so this boundary stays
/// Qt Core only, and so both QML and Qt Style Sheets consume them directly.
/// Both frontends read this table; neither owns a palette of its own.
struct AppearancePalette final {
  Q_GADGET
  /// Primary interactive color, used for selection and emphasis.
  Q_PROPERTY(QString accent MEMBER accent CONSTANT)
  /// Accent under a hovered pointer.
  Q_PROPERTY(QString accentHovered MEMBER accent_hovered CONSTANT)
  /// Accent while a control is pressed.
  Q_PROPERTY(QString accentPressed MEMBER accent_pressed CONSTANT)
  /// Text and icons drawn on an accent-filled surface.
  Q_PROPERTY(QString onAccent MEMBER on_accent CONSTANT)
  /// Application window background.
  Q_PROPERTY(QString window MEMBER window CONSTANT)
  /// Raised panel and view background.
  Q_PROPERTY(QString surface MEMBER surface CONSTANT)
  /// Secondary surface, for alternating or nested panels.
  Q_PROPERTY(QString surfaceAlternate MEMBER surface_alternate CONSTANT)
  /// Control background, such as a button or a text field.
  Q_PROPERTY(QString control MEMBER control CONSTANT)
  /// Control background under a hovered pointer.
  Q_PROPERTY(QString controlHovered MEMBER control_hovered CONSTANT)
  /// Control background while pressed.
  Q_PROPERTY(QString controlPressed MEMBER control_pressed CONSTANT)
  /// Separator and control outline.
  Q_PROPERTY(QString border MEMBER border CONSTANT)
  /// Outline of the control holding keyboard focus.
  Q_PROPERTY(QString focus MEMBER focus CONSTANT)
  /// Text and outlines of a control that cannot be used.
  Q_PROPERTY(QString disabled MEMBER disabled CONSTANT)
  /// Body and heading text.
  Q_PROPERTY(QString textPrimary MEMBER text_primary CONSTANT)
  /// Supporting text, such as descriptions and captions.
  Q_PROPERTY(QString textSecondary MEMBER text_secondary CONSTANT)
  /// Text drawn on an inverted surface.
  Q_PROPERTY(QString textInverted MEMBER text_inverted CONSTANT)
  /// Text reporting a recoverable condition.
  Q_PROPERTY(QString warning MEMBER warning CONSTANT)
  /// Text reporting a failure.
  Q_PROPERTY(QString danger MEMBER danger CONSTANT)
  /// Tooltip background.
  Q_PROPERTY(QString tooltipSurface MEMBER tooltip_surface CONSTANT)
  /// Tooltip text.
  Q_PROPERTY(QString tooltipText MEMBER tooltip_text CONSTANT)

 public:
  /// Primary interactive color, used for selection and emphasis.
  QString accent;
  /// Accent under a hovered pointer.
  QString accent_hovered;
  /// Accent while a control is pressed.
  QString accent_pressed;
  /// Text and icons drawn on an accent-filled surface.
  QString on_accent;

  /// Application window background.
  QString window;
  /// Raised panel and view background.
  QString surface;
  /// Secondary surface, for alternating or nested panels.
  QString surface_alternate;
  /// Control background, such as a button or a text field.
  QString control;
  /// Control background under a hovered pointer.
  QString control_hovered;
  /// Control background while pressed.
  QString control_pressed;

  /// Separator and control outline.
  QString border;
  /// Outline of the control holding keyboard focus.
  QString focus;
  /// Text and outlines of a control that cannot be used.
  QString disabled;

  /// Body and heading text.
  QString text_primary;
  /// Supporting text, such as descriptions and captions.
  QString text_secondary;
  /// Text drawn on an inverted surface.
  QString text_inverted;

  /// Text reporting a recoverable condition.
  QString warning;
  /// Text reporting a failure.
  QString danger;

  /// Tooltip background.
  QString tooltip_surface;
  /// Tooltip text.
  QString tooltip_text;
};

/// Returns the stable, nonlocalized code identifying an appearance.
///
/// These are the values persisted in settings and the ones frontends use to
/// select an appearance, so the vocabulary is the same everywhere.
[[nodiscard]] QString AppearanceCode(ApplicationAppearance appearance);

/// Returns the appearance a code identifies, or following the system when the
/// code is unrecognized.
[[nodiscard]] ApplicationAppearance AppearanceFromCode(const QString& code);

/// Returns the shared palette of a resolved appearance.
[[nodiscard]] const AppearancePalette& AppearancePaletteFor(
    ResolvedAppearance appearance);

/// Owns the appearance preference and the appearance actually presented.
///
/// This is the whole appearance API. The preference is persisted here and
/// nowhere else, so a frontend cannot change it without the change being
/// reported. Frontends bind to this rather than reading settings, so a
/// selection change and a platform change are applied the same way, without
/// restarting the application.
class AppearanceController final : public QObject {
  Q_OBJECT
  /// Appearance the user selected.
  Q_PROPERTY(edit_atlas::presentation::ApplicationAppearance appearance READ
                 Appearance WRITE SetAppearance NOTIFY appearanceChanged)
  /// Appearance currently presented.
  Q_PROPERTY(edit_atlas::presentation::ResolvedAppearance resolvedAppearance
                 READ ResolvedAppearanceValue NOTIFY resolvedAppearanceChanged)
  /// Palette of the presented appearance.
  Q_PROPERTY(edit_atlas::presentation::AppearancePalette palette READ Palette
                 NOTIFY resolvedAppearanceChanged)

 public:
  /// Constructs a controller from the persisted preference.
  ///
  /// Requests the platform color scheme the preference implies, and follows
  /// the platform from then on while the preference is to follow it.
  explicit AppearanceController(QObject* parent = nullptr);
  ~AppearanceController(void) override = default;

  AppearanceController(const AppearanceController&) = delete;
  AppearanceController& operator=(const AppearanceController&) = delete;
  AppearanceController(AppearanceController&&) = delete;
  AppearanceController& operator=(AppearanceController&&) = delete;

  /// Returns the selected appearance.
  [[nodiscard]] ApplicationAppearance Appearance(void) const;

  /// Selects and persists an appearance, applying it immediately.
  void SetAppearance(ApplicationAppearance appearance);

  /// Returns the appearance currently presented.
  [[nodiscard]] ResolvedAppearance ResolvedAppearanceValue(void) const;

  /// Returns the palette of the presented appearance.
  [[nodiscard]] const AppearancePalette& Palette(void) const;

 signals:
  /// Emitted when the selected appearance changes.
  void appearanceChanged(void);

  /// Emitted when the presented appearance changes, whether because the
  /// selection changed or because the platform scheme did.
  void resolvedAppearanceChanged(void);

 private:
  void RefreshResolvedAppearance(void);

  ApplicationAppearance appearance_;
  ResolvedAppearance resolved_;
};

}  // namespace edit_atlas::presentation

#endif  // EDIT_ATLAS_PRESENTATION_APPEARANCE_HPP_
