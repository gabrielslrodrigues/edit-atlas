#ifndef EDIT_ATLAS_PRESENTATION_APPEARANCE_HPP_
#define EDIT_ATLAS_PRESENTATION_APPEARANCE_HPP_

#include <QObject>
#include <QString>

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
    Q_PROPERTY(QString accent MEMBER accent CONSTANT)
    Q_PROPERTY(QString accentHovered MEMBER accentHovered CONSTANT)
    Q_PROPERTY(QString accentPressed MEMBER accentPressed CONSTANT)
    Q_PROPERTY(QString onAccent MEMBER onAccent CONSTANT)
    Q_PROPERTY(QString window MEMBER window CONSTANT)
    Q_PROPERTY(QString surface MEMBER surface CONSTANT)
    Q_PROPERTY(QString surfaceAlternate MEMBER surfaceAlternate CONSTANT)
    Q_PROPERTY(QString control MEMBER control CONSTANT)
    Q_PROPERTY(QString controlHovered MEMBER controlHovered CONSTANT)
    Q_PROPERTY(QString controlPressed MEMBER controlPressed CONSTANT)
    Q_PROPERTY(QString border MEMBER border CONSTANT)
    Q_PROPERTY(QString focus MEMBER focus CONSTANT)
    Q_PROPERTY(QString disabled MEMBER disabled CONSTANT)
    Q_PROPERTY(QString textPrimary MEMBER textPrimary CONSTANT)
    Q_PROPERTY(QString textSecondary MEMBER textSecondary CONSTANT)
    Q_PROPERTY(QString textInverted MEMBER textInverted CONSTANT)
    Q_PROPERTY(QString warning MEMBER warning CONSTANT)
    Q_PROPERTY(QString danger MEMBER danger CONSTANT)
    Q_PROPERTY(QString tooltipSurface MEMBER tooltipSurface CONSTANT)
    Q_PROPERTY(QString tooltipText MEMBER tooltipText CONSTANT)

  public:
    QString accent;
    QString accentHovered;
    QString accentPressed;
    QString onAccent;

    QString window;
    QString surface;
    QString surfaceAlternate;
    QString control;
    QString controlHovered;
    QString controlPressed;

    QString border;
    QString focus;
    QString disabled;

    QString textPrimary;
    QString textSecondary;
    QString textInverted;

    QString warning;
    QString danger;

    QString tooltipSurface;
    QString tooltipText;
};

/// Returns the shared palette of a resolved appearance.
[[nodiscard]] const AppearancePalette &
AppearancePaletteFor(ResolvedAppearance appearance);

/// Owns the appearance preference and the appearance actually presented.
///
/// This is the whole appearance API. The preference is persisted here and
/// nowhere else, so a frontend cannot change it without the change being
/// reported. Frontends bind to this rather than reading settings, so a
/// selection change and a platform change are applied the same way, without
/// restarting the application.
class AppearanceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(edit_atlas::presentation::ApplicationAppearance appearance READ
                   Appearance WRITE SetAppearance NOTIFY appearanceChanged)
    Q_PROPERTY(edit_atlas::presentation::ResolvedAppearance resolvedAppearance
                   READ ResolvedAppearanceValue NOTIFY
                       resolvedAppearanceChanged)
    Q_PROPERTY(edit_atlas::presentation::AppearancePalette palette READ Palette
                   NOTIFY resolvedAppearanceChanged)

  public:
    explicit AppearanceController(QObject *parent = nullptr);
    ~AppearanceController(void) override = default;

    /// Returns the selected appearance.
    [[nodiscard]] ApplicationAppearance Appearance(void) const;

    /// Selects and persists an appearance, applying it immediately.
    void SetAppearance(ApplicationAppearance appearance);

    /// Returns the appearance currently presented.
    [[nodiscard]] ResolvedAppearance ResolvedAppearanceValue(void) const;

    /// Returns the palette of the presented appearance.
    [[nodiscard]] const AppearancePalette &Palette(void) const;

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

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_APPEARANCE_HPP_
