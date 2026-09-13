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

#ifndef EDIT_ATLAS_PRESENTATION_TYPOGRAPHY_HPP_
#define EDIT_ATLAS_PRESENTATION_TYPOGRAPHY_HPP_

#include "QString"
#include "QStringList"

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
  int body_point_size;
  /// Point size of section headings.
  int heading_point_size;
  /// Point size of window and page titles.
  int title_point_size;
  /// Weight of body text.
  int body_weight;
  /// Weight of section headings.
  int heading_weight;
  /// Weight of window and page titles.
  int title_weight;
};

/// Returns the shared typography policy.
[[nodiscard]] const ApplicationTypography& ApplicationTypographyPolicy(void);

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

/// Registers the bundled faces and applies the shared typography.
///
/// Sets the application font family and body point size, leaving the platform
/// family in place when the bundled one is unavailable. Both frontends call
/// this so neither derives a font of its own.
void ApplyApplicationTypography(void);

/// Returns the family a frontend should apply.
///
/// This is the bundled family once registration has succeeded, and an empty
/// string otherwise, which leaves the platform default in place.
[[nodiscard]] QString ResolvedTypographyFamily(void);

}  // namespace edit_atlas::presentation

#endif  // EDIT_ATLAS_PRESENTATION_TYPOGRAPHY_HPP_
