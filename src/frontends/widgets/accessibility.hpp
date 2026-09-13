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

#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_ACCESSIBILITY_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_ACCESSIBILITY_HPP_

#include "QAction"
#include "QInputDialog"
#include "QStringView"
#include "QWidget"

namespace edit_atlas::frontends::widgets {

/// Installs desktop accessibility interfaces for application controls whose
/// native Qt interfaces do not expose complete semantic operations.
void InstallApplicationAccessibility(void);

/// Assigns one stable, nonlocalized identifier to an action's Qt identity and
/// accessibility metadata.
void SetAutomationIdentifier(QAction& action, QStringView identifier);

/// Assigns one stable, nonlocalized identifier to a widget's Qt and
/// accessibility identities.
void SetAutomationIdentifier(QWidget& widget, QStringView identifier);

/// Assigns stable identifiers to the lazily created buttons of an input
/// dialog when its event loop starts.
void SetInputDialogButtonAutomationIdentifiers(QInputDialog& dialog,
                                               QStringView accept_identifier,
                                               QStringView cancel_identifier);

/// Assigns accessibility identity without replacing a widget object name used
/// for another purpose, such as stylesheet selection.
void SetAccessibilityIdentifier(QWidget& widget, QStringView identifier);

}  // namespace edit_atlas::frontends::widgets

#endif  // EDIT_ATLAS_FRONTENDS_WIDGETS_ACCESSIBILITY_HPP_
