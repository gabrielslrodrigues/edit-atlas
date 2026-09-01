#ifndef EDIT_ATLAS_FRONTENDS_QUICK_STYLE_APPEARANCE_STYLE_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_STYLE_APPEARANCE_STYLE_HPP_

#include <edit_atlas/presentation/appearance.hpp>

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

namespace edit_atlas::frontends::quick::style {

/// Registers the shared palette as a QML value type.
///
/// `edit_atlas::presentation` is Qt Core only and carries no QML macros, so
/// the registration lives here. Without it QML knows the property's type name
/// but nothing about its members, and every color reads as undefined.
// Not `final`: QML type registration derives from a registered creatable
// type, so marking one final fails to compile under MSVC. The application's
// other QML types avoid this by being QML_UNCREATABLE.
struct AppearancePaletteForeign {
    Q_GADGET
    QML_FOREIGN(edit_atlas::presentation::AppearancePalette)
    QML_VALUE_TYPE(appearancePalette)
};

/// Exposes the shared appearance state to QML as `Appearance`.
///
/// The style module owns no palette of its own: this forwards the one in
/// `edit_atlas::presentation`, so both frontends present the same colors and
/// a change reaches every binding through the property system.
class AppearanceStyle : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Appearance)
    QML_SINGLETON
    Q_PROPERTY(QString appearanceCode READ AppearanceCode WRITE
                   SetAppearanceCode NOTIFY appearanceChanged)
    Q_PROPERTY(edit_atlas::presentation::AppearancePalette palette READ Palette
                   NOTIFY paletteChanged)

  public:
    explicit AppearanceStyle(QObject *parent = nullptr);
    ~AppearanceStyle(void) override = default;

    /// Returns the stable code of the selected appearance.
    ///
    /// Views select an appearance by code rather than by enumerator, as they
    /// select a language, so no enumeration has to be registered with QML.
    [[nodiscard]] QString AppearanceCode(void) const;

    /// Selects an appearance by code and persists it.
    void SetAppearanceCode(const QString &code);

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
