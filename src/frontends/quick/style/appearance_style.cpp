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

#include "edit_atlas/frontends/quick/style/appearance_style.hpp"

#include "QObject"
#include "QString"

#include "edit_atlas/presentation/appearance.hpp"

namespace edit_atlas::frontends::quick::style {

AppearanceStyle::AppearanceStyle(QObject* parent) : QObject{parent} {
  connect(&controller_, &presentation::AppearanceController::appearanceChanged,
          this, &AppearanceStyle::appearanceChanged);
  connect(&controller_,
          &presentation::AppearanceController::resolvedAppearanceChanged, this,
          &AppearanceStyle::paletteChanged);
}

QString AppearanceStyle::AppearanceCode(void) const {
  return presentation::AppearanceCode(controller_.Appearance());
}

void AppearanceStyle::SetAppearanceCode(const QString& code) {
  controller_.SetAppearance(presentation::AppearanceFromCode(code));
}

presentation::AppearancePalette AppearanceStyle::Palette(void) const {
  return controller_.Palette();
}

}  // namespace edit_atlas::frontends::quick::style
