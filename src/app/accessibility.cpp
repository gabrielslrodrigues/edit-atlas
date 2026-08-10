#include "accessibility.hpp"

#include <QAction>
#include <QAbstractItemView>
#include <QAccessible>
#include <QAccessibleWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace edit_atlas::app {

#if defined(Q_OS_LINUX)
namespace {

class AccessibleComboBox final : public QAccessibleWidget,
                                 public QAccessibleValueInterface {
public:
    explicit AccessibleComboBox(QComboBox *combo_box)
        : QAccessibleWidget{combo_box, QAccessible::ComboBox} {}

    void *interface_cast(QAccessible::InterfaceType type) override {
        if (type == QAccessible::ValueInterface) {
            return static_cast<QAccessibleValueInterface *>(this);
        }
        return QAccessibleWidget::interface_cast(type);
    }

    QAccessibleInterface *child(int index) const override {
        if (index == 0) {
            return QAccessible::queryAccessibleInterface(comboBox()->view());
        }
        if (index == 1 && comboBox()->isEditable()) {
            return QAccessible::queryAccessibleInterface(comboBox()->lineEdit());
        }
        return nullptr;
    }

    int childCount(void) const override {
        return comboBox()->isEditable() ? 2 : 1;
    }

    int indexOfChild(const QAccessibleInterface *child_interface) const override {
        if (child_interface == nullptr) {
            return -1;
        }
        if (child_interface->object() == comboBox()->view()) {
            return 0;
        }
        if (comboBox()->isEditable() &&
            child_interface->object() == comboBox()->lineEdit()) {
            return 1;
        }
        return -1;
    }

    QAccessibleInterface *focusChild(void) const override {
        return comboBox()->isEditable() ? child(1) : nullptr;
    }

    QString text(QAccessible::Text type) const override {
        if (type == QAccessible::Name || type == QAccessible::Value) {
            return comboBox()->isEditable() ? comboBox()->lineEdit()->text()
                                            : comboBox()->currentText();
        }
        if (type == QAccessible::Accelerator) {
            return QKeySequence{Qt::Key_Down}.toString(
                QKeySequence::NativeText);
        }
        return QAccessibleWidget::text(type);
    }

    QAccessible::State state(void) const override {
        auto accessible_state = QAccessibleWidget::state();
        accessible_state.expandable = true;
        accessible_state.expanded = comboBox()->view()->isVisible();
        accessible_state.editable = comboBox()->isEditable();
        return accessible_state;
    }

    QStringList actionNames(void) const override {
        return {showMenuAction(), pressAction()};
    }

    QString localizedActionDescription(const QString &action_name) const override {
        if (action_name == showMenuAction() || action_name == pressAction()) {
            return QComboBox::tr("Open the combo box selection popup");
        }
        return {};
    }

    void doAction(const QString &action_name) override {
        if (action_name != showMenuAction() && action_name != pressAction()) {
            return;
        }
        if (comboBox()->view()->isVisible()) {
            comboBox()->hidePopup();
        } else {
            comboBox()->showPopup();
        }
    }

    QStringList keyBindingsForAction(const QString &) const override {
        return {};
    }

    QVariant currentValue(void) const override {
        return comboBox()->currentIndex();
    }

    void setCurrentValue(const QVariant &value) override {
        bool converted = false;
        const auto requested = value.toDouble(&converted);
        if (!converted || requested < 0.0 ||
            requested >= static_cast<double>(comboBox()->count())) {
            return;
        }
        const auto index = static_cast<int>(requested);
        if (requested == static_cast<double>(index)) {
            comboBox()->setCurrentIndex(index);
        }
    }

    QVariant maximumValue(void) const override {
        return comboBox()->count() > 0 ? comboBox()->count() - 1 : 0;
    }

    QVariant minimumValue(void) const override { return 0; }

    QVariant minimumStepSize(void) const override { return 1; }

private:
    QComboBox *comboBox(void) const {
        return qobject_cast<QComboBox *>(object());
    }
};

class AccessibleTemplateActionsButton final : public QAccessibleWidget {
public:
    explicit AccessibleTemplateActionsButton(QToolButton *button)
        : QAccessibleWidget{button, QAccessible::Button} {}

    QStringList actionNames(void) const override {
        QStringList names{showMenuAction()};
        if (button()->menu() == nullptr) {
            return names;
        }
        for (const auto *action : button()->menu()->actions()) {
            const auto identifier =
                action->property("accessibleIdentifier").toString();
            if (!identifier.isEmpty()) {
                names.push_back(identifier);
            }
        }
        return names;
    }

    QString localizedActionDescription(
        const QString &action_name) const override {
        if (action_name == showMenuAction()) {
            return QToolButton::tr("Open the template actions menu");
        }
        if (const auto *action = findAction(action_name); action != nullptr) {
            return action->text();
        }
        return {};
    }

    void doAction(const QString &action_name) override {
        if (action_name == showMenuAction()) {
            auto *control = button();
            QTimer::singleShot(0, control, [control] { control->showMenu(); });
            return;
        }
        if (auto *action = findAction(action_name); action != nullptr) {
            QTimer::singleShot(0, action, &QAction::trigger);
        }
    }

    QStringList keyBindingsForAction(const QString &) const override {
        return {};
    }

private:
    QToolButton *button(void) const {
        return qobject_cast<QToolButton *>(object());
    }

    QAction *findAction(const QString &identifier) const {
        if (button()->menu() == nullptr) {
            return nullptr;
        }
        for (auto *action : button()->menu()->actions()) {
            if (action->property("accessibleIdentifier").toString() ==
                identifier) {
                return action;
            }
        }
        return nullptr;
    }
};

class AccessibleProjectionList;

class AccessibleProjectionItem final : public QAccessibleInterface,
                                       public QAccessibleActionInterface {
public:
    AccessibleProjectionItem(AccessibleProjectionList *parent,
                             QListWidgetItem *item)
        : parent_{parent}, item_{item} {}

    bool isValid(void) const override;
    QObject *object(void) const override { return nullptr; }
    QWindow *window(void) const override;
    QAccessibleInterface *childAt(int, int) const override { return nullptr; }
    QAccessibleInterface *parent(void) const override;
    QAccessibleInterface *child(int) const override { return nullptr; }
    int childCount(void) const override { return 0; }
    int indexOfChild(const QAccessibleInterface *) const override { return -1; }
    QString text(QAccessible::Text type) const override;
    void setText(QAccessible::Text, const QString &) override {}
    QRect rect(void) const override;
    QAccessible::Role role(void) const override {
        return QAccessible::ListItem;
    }
    QAccessible::State state(void) const override;
    void *interface_cast(QAccessible::InterfaceType type) override {
        if (type == QAccessible::ActionInterface) {
            return static_cast<QAccessibleActionInterface *>(this);
        }
        return nullptr;
    }
    QStringList actionNames(void) const override {
        return {pressAction(), toggleAction()};
    }
    QString localizedActionDescription(
        const QString &action_name) const override;
    void doAction(const QString &action_name) override;
    QStringList keyBindingsForAction(const QString &) const override {
        return {};
    }

    QListWidgetItem *item(void) const { return item_; }

private:
    AccessibleProjectionList *parent_;
    QListWidgetItem *item_;
};

class AccessibleProjectionList final : public QAccessibleWidget,
                                       public QAccessibleSelectionInterface {
public:
    explicit AccessibleProjectionList(QListWidget *list)
        : QAccessibleWidget{list, QAccessible::List} {}

    void *interface_cast(QAccessible::InterfaceType type) override {
        if (type == QAccessible::SelectionInterface) {
            return static_cast<QAccessibleSelectionInterface *>(this);
        }
        return QAccessibleWidget::interface_cast(type);
    }

    QAccessibleInterface *childAt(int x, int y) const override {
        for (int index = 0; index < childCount(); ++index) {
            auto *candidate = child(index);
            if (candidate != nullptr && candidate->rect().contains(x, y)) {
                return candidate;
            }
        }
        return nullptr;
    }

    QAccessibleInterface *child(int index) const override {
        if (index < 0 || index >= list()->count()) {
            return nullptr;
        }
        auto *item = list()->item(index);
        const auto match = std::ranges::find_if(
            children_, [item](const auto &candidate) {
                return candidate->item() == item;
            });
        if (match != children_.end()) {
            return match->get();
        }
        children_.push_back(
            std::make_unique<AccessibleProjectionItem>(
                const_cast<AccessibleProjectionList *>(this), item));
        return children_.back().get();
    }

    int childCount(void) const override { return list()->count(); }

    int indexOfChild(const QAccessibleInterface *child_interface) const override {
        const auto *item_interface =
            dynamic_cast<const AccessibleProjectionItem *>(child_interface);
        return item_interface == nullptr
                   ? -1
                   : list()->row(item_interface->item());
    }

    QAccessibleInterface *focusChild(void) const override {
        const auto row = list()->currentRow();
        return row < 0 ? nullptr : child(row);
    }

    int selectedItemCount(void) const override {
        return static_cast<int>(list()->selectedItems().size());
    }

    QList<QAccessibleInterface *> selectedItems(void) const override {
        QList<QAccessibleInterface *> selected;
        for (auto *item : list()->selectedItems()) {
            selected.push_back(child(list()->row(item)));
        }
        return selected;
    }

    bool select(QAccessibleInterface *child_interface) override {
        auto *item_interface =
            dynamic_cast<AccessibleProjectionItem *>(child_interface);
        if (item_interface == nullptr) {
            return false;
        }
        list()->setCurrentItem(item_interface->item(),
                               QItemSelectionModel::ClearAndSelect);
        return true;
    }

    bool unselect(QAccessibleInterface *child_interface) override {
        auto *item_interface =
            dynamic_cast<AccessibleProjectionItem *>(child_interface);
        if (item_interface == nullptr) {
            return false;
        }
        item_interface->item()->setSelected(false);
        return true;
    }

    bool selectAll(void) override {
        if (list()->selectionMode() != QAbstractItemView::MultiSelection &&
            list()->selectionMode() != QAbstractItemView::ExtendedSelection) {
            return false;
        }
        list()->selectAll();
        return true;
    }

    bool clear(void) override {
        list()->clearSelection();
        return true;
    }

    QListWidget *list(void) const {
        return qobject_cast<QListWidget *>(object());
    }

private:
    mutable std::vector<std::unique_ptr<AccessibleProjectionItem>> children_;
};

bool AccessibleProjectionItem::isValid(void) const {
    return parent_ != nullptr && parent_->list() != nullptr && item_ != nullptr &&
           parent_->list()->row(item_) >= 0;
}

QWindow *AccessibleProjectionItem::window(void) const {
    return parent_->window();
}

QAccessibleInterface *AccessibleProjectionItem::parent(void) const {
    return parent_;
}

QString AccessibleProjectionItem::text(QAccessible::Text type) const {
    return type == QAccessible::Name ? item_->text() : QString{};
}

QRect AccessibleProjectionItem::rect(void) const {
    auto *list = parent_->list();
    auto item_rect = list->visualItemRect(item_);
    item_rect.moveTopLeft(list->viewport()->mapToGlobal(item_rect.topLeft()));
    return item_rect;
}

QAccessible::State AccessibleProjectionItem::state(void) const {
    QAccessible::State accessible_state;
    const auto *list = parent_->list();
    accessible_state.disabled = !(item_->flags() & Qt::ItemIsEnabled);
    accessible_state.selectable = bool(item_->flags() & Qt::ItemIsSelectable);
    accessible_state.selected = item_->isSelected();
    accessible_state.focusable = accessible_state.selectable;
    accessible_state.focused = list->hasFocus() && list->currentItem() == item_;
    accessible_state.checkable = bool(item_->flags() & Qt::ItemIsUserCheckable);
    accessible_state.checked = item_->checkState() == Qt::Checked;
    accessible_state.checkStateMixed =
        item_->checkState() == Qt::PartiallyChecked;
    const auto visible = list->isVisible() &&
                         list->viewport()->rect().intersects(
                             list->visualItemRect(item_));
    accessible_state.invisible = !visible;
    accessible_state.offscreen = !visible;
    return accessible_state;
}

QString AccessibleProjectionItem::localizedActionDescription(
    const QString &action_name) const {
    if (action_name == pressAction()) {
        return QListWidget::tr("Select this export column");
    }
    if (action_name == toggleAction()) {
        return QListWidget::tr("Include or exclude this export column");
    }
    return {};
}

void AccessibleProjectionItem::doAction(const QString &action_name) {
    auto *list = parent_->list();
    if (action_name == pressAction()) {
        list->setCurrentItem(item_, QItemSelectionModel::ClearAndSelect);
    } else if (action_name == toggleAction() &&
               item_->flags() & Qt::ItemIsUserCheckable) {
        item_->setCheckState(item_->checkState() == Qt::Checked ? Qt::Unchecked
                                                                : Qt::Checked);
    }
}

QAccessibleInterface *CreateApplicationAccessibleInterface(const QString &,
                                                            QObject *object) {
    if (auto *combo_box = qobject_cast<QComboBox *>(object);
        combo_box != nullptr) {
        return new AccessibleComboBox{combo_box};
    }
    if (auto *button = qobject_cast<QToolButton *>(object);
        button != nullptr &&
        button->objectName() == QStringLiteral("templateActionsButton")) {
        return new AccessibleTemplateActionsButton{button};
    }
    if (auto *list = qobject_cast<QListWidget *>(object); list != nullptr) {
        return new AccessibleProjectionList{list};
    }
    return nullptr;
}

} // namespace
#endif

void InstallApplicationAccessibility(void) {
#if defined(Q_OS_LINUX)
    QAccessible::installFactory(CreateApplicationAccessibleInterface);
#endif
}

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

void SetInputDialogButtonAutomationIdentifiers(
    QInputDialog &dialog, QStringView accept_identifier,
    QStringView cancel_identifier) {
    const auto accept_value = accept_identifier.toString();
    const auto cancel_value = cancel_identifier.toString();
    auto assign_identifiers = [&dialog, accept_value, cancel_value] {
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        if (buttons == nullptr) {
            return;
        }
        if (auto *accept = buttons->button(QDialogButtonBox::Ok);
            accept != nullptr) {
            SetAutomationIdentifier(*accept, accept_value);
        }
        if (auto *cancel = buttons->button(QDialogButtonBox::Cancel);
            cancel != nullptr) {
            SetAutomationIdentifier(*cancel, cancel_value);
        }
    };
    assign_identifiers();
    QTimer::singleShot(0, &dialog, std::move(assign_identifiers));
}

void SetAccessibilityIdentifier(QWidget &widget, QStringView identifier) {
    widget.setAccessibleIdentifier(identifier.toString());
}

} // namespace edit_atlas::app
