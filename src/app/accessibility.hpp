#ifndef EDIT_ATLAS_APP_ACCESSIBILITY_HPP_
#define EDIT_ATLAS_APP_ACCESSIBILITY_HPP_

#include <QStringView>

class QAction;
class QInputDialog;
class QWidget;

namespace edit_atlas::app {

/// Installs Linux accessibility interfaces for application controls whose
/// native Qt interfaces do not expose complete semantic operations.
void InstallApplicationAccessibility(void);

/// Assigns one stable, nonlocalized identifier to an action's Qt identity and
/// accessibility metadata.
void SetAutomationIdentifier(QAction &action, QStringView identifier);

/// Assigns one stable, nonlocalized identifier to a widget's Qt and
/// accessibility identities.
void SetAutomationIdentifier(QWidget &widget, QStringView identifier);

/// Assigns stable identifiers to the lazily created buttons of an input
/// dialog when its event loop starts.
void SetInputDialogButtonAutomationIdentifiers(
    QInputDialog &dialog, QStringView accept_identifier,
    QStringView cancel_identifier);

/// Assigns accessibility identity without replacing a widget object name used
/// for another purpose, such as stylesheet selection.
void SetAccessibilityIdentifier(QWidget &widget, QStringView identifier);

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_ACCESSIBILITY_HPP_
