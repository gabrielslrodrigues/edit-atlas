#include "event_projection_dialog.hpp"

#include "accessibility.hpp"
#include "event_projection_widget.hpp"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <span>
#include <vector>

namespace edit_atlas::app {

EventProjectionDialog::EventProjectionDialog(
    std::span<const core::TimelineEventField> projection, QWidget *parent)
    : QDialog{parent} {
    setWindowTitle(tr("Export Columns"));
    setModal(true);
    resize(560, 620);
    auto *layout = new QVBoxLayout{this};
    projection_ = new EventProjectionWidget{projection, this};
    layout->addWidget(projection_, 1);
    auto *buttons = new QDialogButtonBox{this};
    save_ = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
    auto *cancel =
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);

    SetAutomationIdentifier(*this, u"eventProjectionDialog");
    SetAutomationIdentifier(*save_, u"saveProjectionButton");
    SetAutomationIdentifier(*cancel, u"cancelProjectionButton");

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(projection_, &EventProjectionWidget::ValidityChanged, save_,
            &QPushButton::setEnabled);
    save_->setEnabled(!projection_->Projection().empty());
    layout->addWidget(buttons);
}

std::vector<core::TimelineEventField>
EventProjectionDialog::Projection(void) const {
    return projection_->Projection();
}

} // namespace edit_atlas::app
