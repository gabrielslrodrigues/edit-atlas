#include "accessibility.hpp"

#include <QAction>
#include <QString>
#include <QStringView>
#include <QVariant>
#include <QWidget>

namespace edit_atlas::app {

void SetAutomationIdentifier(QAction &action, QStringView identifier) {
    const auto value = identifier.toString();
    action.setObjectName(value);
    action.setProperty("accessibleIdentifier", value);
}

void SetAutomationIdentifier(QWidget &widget, QStringView identifier) {
    const auto value = identifier.toString();
    widget.setObjectName(value);
    widget.setAccessibleIdentifier(value);
}

void SetAccessibilityIdentifier(QWidget &widget, QStringView identifier) {
    widget.setAccessibleIdentifier(identifier.toString());
}

} // namespace edit_atlas::app
