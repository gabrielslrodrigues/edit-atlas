#ifndef EDIT_ATLAS_FRONTENDS_QUICK_STYLE_TYPOGRAPHY_STYLE_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_STYLE_TYPOGRAPHY_STYLE_HPP_

#include <edit_atlas/presentation/typography.hpp>

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

namespace edit_atlas::frontends::quick::style {

/// Exposes the shared typography policy to QML as `Typography`.
///
/// The style module defines no font of its own: sizes and weights come from
/// `edit_atlas::presentation`, so both frontends render the same hierarchy.
/// Not `final`: QML registration derives from a registered creatable type.
class TypographyStyle : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Typography)
    QML_SINGLETON
    /// Family applied to the interface, empty when the platform default is
    /// kept because the bundled family is unavailable.
    Q_PROPERTY(QString family READ Family CONSTANT)
    /// Point size of body text.
    Q_PROPERTY(int bodyPointSize READ BodyPointSize CONSTANT)
    /// Point size of section headings.
    Q_PROPERTY(int headingPointSize READ HeadingPointSize CONSTANT)
    /// Point size of window and page titles.
    Q_PROPERTY(int titlePointSize READ TitlePointSize CONSTANT)
    /// Weight of body text.
    Q_PROPERTY(int bodyWeight READ BodyWeight CONSTANT)
    /// Weight of section headings.
    Q_PROPERTY(int headingWeight READ HeadingWeight CONSTANT)
    /// Weight of window and page titles.
    Q_PROPERTY(int titleWeight READ TitleWeight CONSTANT)

  public:
    /// Constructs the QML-facing view of the shared policy.
    explicit TypographyStyle(QObject *parent = nullptr);
    ~TypographyStyle(void) override = default;

    /// Returns the family to apply, or an empty string for the platform one.
    [[nodiscard]] QString Family(void) const;

    /// Returns the point size of body text.
    [[nodiscard]] int BodyPointSize(void) const;

    /// Returns the point size of section headings.
    [[nodiscard]] int HeadingPointSize(void) const;

    /// Returns the point size of window and page titles.
    [[nodiscard]] int TitlePointSize(void) const;

    /// Returns the weight of body text.
    [[nodiscard]] int BodyWeight(void) const;

    /// Returns the weight of section headings.
    [[nodiscard]] int HeadingWeight(void) const;

    /// Returns the weight of window and page titles.
    [[nodiscard]] int TitleWeight(void) const;
};

} // namespace edit_atlas::frontends::quick::style

#endif // EDIT_ATLAS_FRONTENDS_QUICK_STYLE_TYPOGRAPHY_STYLE_HPP_
