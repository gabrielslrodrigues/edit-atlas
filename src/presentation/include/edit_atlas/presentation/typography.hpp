#ifndef EDIT_ATLAS_PRESENTATION_TYPOGRAPHY_HPP_
#define EDIT_ATLAS_PRESENTATION_TYPOGRAPHY_HPP_

#include <QString>
#include <QStringList>

namespace edit_atlas::presentation {

/// Interface typography shared by both graphical frontends.
///
/// Weights are on the 100 to 900 scale Qt and CSS both use, and sizes are in
/// points so device-independent scaling and high-DPI behavior are unchanged.
/// Neither frontend defines a font of its own.
struct ApplicationTypography final {
    /// Bundled family applied when its faces registered successfully.
    QString family;
    /// Point size of body text.
    int bodyPointSize;
    /// Point size of section headings.
    int headingPointSize;
    /// Point size of window and page titles.
    int titlePointSize;
    /// Weight of body text.
    int bodyWeight;
    /// Weight of section headings.
    int headingWeight;
    /// Weight of window and page titles.
    int titleWeight;
};

/// Returns the shared typography policy.
[[nodiscard]] const ApplicationTypography &ApplicationTypographyPolicy(void);

/// Resource paths of the bundled faces, in the order they are registered.
///
/// A graphical application embeds these through
/// `edit_atlas_add_application_icon_resource`; the CLI embeds none of them.
[[nodiscard]] QStringList BundledTypographyResourcePaths(void);

/// Registers the bundled faces with the font database.
///
/// Returns whether the bundled family is usable afterwards. A frontend that
/// receives `false` must keep the platform's sans-serif family rather than
/// naming a family the platform cannot resolve, which is why this reports a
/// result instead of failing.
[[nodiscard]] bool RegisterBundledTypography(void);

/// Returns the family a frontend should apply.
///
/// This is the bundled family once registration has succeeded, and an empty
/// string otherwise, which leaves the platform default in place.
[[nodiscard]] QString ResolvedTypographyFamily(void);

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TYPOGRAPHY_HPP_
