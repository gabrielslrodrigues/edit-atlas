#ifndef EDIT_ATLAS_APP_ACCESSIBILITY_HPP_
#define EDIT_ATLAS_APP_ACCESSIBILITY_HPP_

#include <QStringView>

class QAction;
class QWidget;

namespace edit_atlas::app {

/// Assigns one stable, nonlocalized identifier to an action's Qt identity and
/// accessibility metadata.
void SetAutomationIdentifier(QAction &action, QStringView identifier);

/// Assigns one stable, nonlocalized identifier to a widget's Qt and
/// accessibility identities.
void SetAutomationIdentifier(QWidget &widget, QStringView identifier);

/// Assigns accessibility identity without replacing a widget object name used
/// for another purpose, such as stylesheet selection.
void SetAccessibilityIdentifier(QWidget &widget, QStringView identifier);

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_ACCESSIBILITY_HPP_
