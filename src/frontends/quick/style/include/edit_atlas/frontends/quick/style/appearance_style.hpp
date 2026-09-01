#ifndef EDIT_ATLAS_FRONTENDS_QUICK_STYLE_APPEARANCE_STYLE_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_STYLE_APPEARANCE_STYLE_HPP_

#include <edit_atlas/presentation/appearance.hpp>

#include <QObject>
#include <QtQmlIntegration>

namespace edit_atlas::frontends::quick::style {

/// Exposes the shared appearance state to QML as `Appearance`.
///
/// The style module owns no palette of its own: this forwards the one in
/// `edit_atlas::presentation`, so both frontends present the same colors and
/// a change reaches every binding through the property system.
class AppearanceStyle final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Appearance)
    QML_SINGLETON
    Q_PROPERTY(edit_atlas::presentation::ApplicationAppearance appearance READ
                   Appearance WRITE SetAppearance NOTIFY appearanceChanged)
    Q_PROPERTY(edit_atlas::presentation::AppearancePalette palette READ Palette
                   NOTIFY paletteChanged)

  public:
    explicit AppearanceStyle(QObject *parent = nullptr);
    ~AppearanceStyle(void) override = default;

    /// Returns the selected appearance.
    [[nodiscard]] presentation::ApplicationAppearance Appearance(void) const;

    /// Selects an appearance and persists it.
    void SetAppearance(presentation::ApplicationAppearance appearance);

    /// Returns the palette of the presented appearance.
    [[nodiscard]] presentation::AppearancePalette Palette(void) const;

  signals:
    /// Emitted when the selected appearance changes.
    void appearanceChanged(void);

    /// Emitted when the presented palette changes, whether because the
    /// selection changed or because the platform scheme did.
    void paletteChanged(void);

  private:
    presentation::AppearanceController controller_;
};

} // namespace edit_atlas::frontends::quick::style

#endif // EDIT_ATLAS_FRONTENDS_QUICK_STYLE_APPEARANCE_STYLE_HPP_
