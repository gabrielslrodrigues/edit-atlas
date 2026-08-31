#include "event_projection_widget.hpp"

#include "accessibility.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include <cstddef>
#include <span>
#include <vector>

namespace edit_atlas::frontends::widgets {
namespace {

[[nodiscard]] QString ColumnText(core::TimelineEventField field) {
    switch (field) {
    case core::TimelineEventField::kEventIdentifier:
        return EventProjectionWidget::tr("Event");
    case core::TimelineEventField::kInitialFrame:
        return EventProjectionWidget::tr("Initial frame");
    case core::TimelineEventField::kReel:
        return EventProjectionWidget::tr("Reel");
    case core::TimelineEventField::kTrackKind:
        return EventProjectionWidget::tr("Track type");
    case core::TimelineEventField::kTrackIdentifier:
        return EventProjectionWidget::tr("Track");
    case core::TimelineEventField::kEditType:
        return EventProjectionWidget::tr("Edit type");
    case core::TimelineEventField::kTransitionIdentifier:
        return EventProjectionWidget::tr("Transition");
    case core::TimelineEventField::kTransitionDuration:
        return EventProjectionWidget::tr("Transition frames");
    case core::TimelineEventField::kSourceIn:
        return EventProjectionWidget::tr("Source in");
    case core::TimelineEventField::kSourceOut:
        return EventProjectionWidget::tr("Source out");
    case core::TimelineEventField::kRecordIn:
        return EventProjectionWidget::tr("Record in");
    case core::TimelineEventField::kRecordOut:
        return EventProjectionWidget::tr("Record out");
    case core::TimelineEventField::kDuration:
        return EventProjectionWidget::tr("Duration");
    case core::TimelineEventField::kDurationFrames:
        return EventProjectionWidget::tr("Duration frames");
    case core::TimelineEventField::kClipName:
        return EventProjectionWidget::tr("Clip name");
    case core::TimelineEventField::kSourceFile:
        return EventProjectionWidget::tr("Source file");
    case core::TimelineEventField::kComments:
        return EventProjectionWidget::tr("Comments");
    case core::TimelineEventField::kSourceLine:
        return EventProjectionWidget::tr("Source line");
    case core::TimelineEventField::kCount:
        break;
    }
    return {};
}

} // namespace

EventProjectionWidget::EventProjectionWidget(
    std::span<const core::TimelineEventField> projection, QWidget *parent)
    : QWidget{parent} {
    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);
    auto *label = new QLabel{tr("Event columns"), this};
    layout->addWidget(label);
    auto *description = new QLabel{
        tr("Select the columns to export and arrange them in output order."),
        this};
    description->setWordWrap(true);
    layout->addWidget(description);

    columns_ = new QListWidget{this};
    columns_->setAccessibleName(tr("Event columns"));
    SetAutomationIdentifier(*columns_, u"eventColumnsList");
    for (const auto field : core::TimelineEventFields()) {
        auto *item = new QListWidgetItem{ColumnText(field), columns_};
        item->setData(Qt::UserRole, static_cast<int>(field));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    for (auto position = projection.rbegin(); position != projection.rend();
         ++position) {
        for (int row = 0; row < columns_->count(); ++row) {
            auto *item = columns_->item(row);
            if (static_cast<core::TimelineEventField>(
                    item->data(Qt::UserRole).toInt()) == *position) {
                item->setCheckState(Qt::Checked);
                auto *selected = columns_->takeItem(row);
                columns_->insertItem(0, selected);
                break;
            }
        }
    }
    columns_->setCurrentRow(0);
    layout->addWidget(columns_, 1);

    auto *buttons = new QHBoxLayout;
    move_up_ = new QPushButton{tr("Move up"), this};
    move_down_ = new QPushButton{tr("Move down"), this};
    buttons->addWidget(move_up_);
    buttons->addWidget(move_down_);
    buttons->addStretch();
    layout->addLayout(buttons);

    error_ = new QLabel{tr("Select at least one event column."), this};
    layout->addWidget(error_);

    SetAutomationIdentifier(*this, u"eventProjectionWidget");
    SetAutomationIdentifier(*move_up_, u"moveColumnUpButton");
    SetAutomationIdentifier(*move_down_, u"moveColumnDownButton");
    SetAutomationIdentifier(*error_, u"columnSelectionErrorLabel");

    connect(move_up_, &QPushButton::clicked, this,
            [this](void) { MoveCurrentColumn(-1); });
    connect(move_down_, &QPushButton::clicked, this,
            [this](void) { MoveCurrentColumn(1); });
    connect(columns_, &QListWidget::currentRowChanged, this,
            [this](int) { UpdateControls(); });
    connect(columns_, &QListWidget::itemChanged, this,
            [this](QListWidgetItem *) { UpdateControls(); });
    UpdateControls();
}

std::vector<core::TimelineEventField>
EventProjectionWidget::Projection(void) const {
    std::vector<core::TimelineEventField> projection;
    projection.reserve(static_cast<std::size_t>(columns_->count()));
    for (int row = 0; row < columns_->count(); ++row) {
        const auto *item = columns_->item(row);
        if (item->checkState() == Qt::Checked) {
            projection.push_back(static_cast<core::TimelineEventField>(
                item->data(Qt::UserRole).toInt()));
        }
    }
    return projection;
}

void EventProjectionWidget::MoveCurrentColumn(int offset) {
    const auto current_row = columns_->currentRow();
    const auto target_row = current_row + offset;
    if (current_row < 0 || target_row < 0 || target_row >= columns_->count()) {
        return;
    }
    auto *item = columns_->takeItem(current_row);
    columns_->insertItem(target_row, item);
    columns_->setCurrentRow(target_row);
    UpdateControls();
}

void EventProjectionWidget::UpdateControls(void) {
    const auto current_row = columns_->currentRow();
    move_up_->setEnabled(current_row > 0);
    move_down_->setEnabled(current_row >= 0 &&
                           current_row + 1 < columns_->count());
    const auto valid = !Projection().empty();
    error_->setVisible(!valid);
    emit ProjectionChanged();
    emit ValidityChanged(valid);
}

} // namespace edit_atlas::frontends::widgets
