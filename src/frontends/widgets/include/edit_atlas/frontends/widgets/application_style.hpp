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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_STYLE_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_STYLE_HPP_

#include "QApplication"
#include "QPalette"
#include "QString"

#include "edit_atlas/presentation/appearance.hpp"

namespace edit_atlas::frontends::widgets {

/// Loads the embedded stylesheet with the palette's colors substituted.
///
/// The stylesheet is a template: it names shared appearance tokens rather
/// than colors, so light and dark differ only by the table applied to it.
/// Returns an empty string when the resource cannot be read, and leaves an
/// unrecognized token in place so it is visible rather than silently blank.
[[nodiscard]] QString LoadApplicationStyleSheet(
    const presentation::AppearancePalette& palette);

/// Builds the widget palette for one shared appearance palette.
[[nodiscard]] QPalette BuildApplicationPalette(
    const presentation::AppearancePalette& palette);

/// Applies a palette and its stylesheet to a running application.
///
/// Safe to call repeatedly: it is how an appearance change is applied without
/// restarting.
void ApplyApplicationAppearance(QApplication& application,
                                const presentation::AppearancePalette& palette);

/// Applies the compact application behavior, then the given appearance.
void ApplyApplicationStyle(QApplication& application,
                           const presentation::AppearancePalette& palette);

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_APPLICATION_STYLE_HPP_
