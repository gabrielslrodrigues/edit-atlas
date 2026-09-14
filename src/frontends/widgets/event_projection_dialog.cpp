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

#include "event_projection_dialog.hpp"

#include <span>
#include <vector>

#include "QDialogButtonBox"
#include "QPushButton"
#include "QVBoxLayout"
#include "QWidget"

#include "accessibility.hpp"
#include "edit_atlas/core/timeline_projection.hpp"
#include "event_projection_widget.hpp"

namespace edit_atlas::frontends::widgets {

EventProjectionDialog::EventProjectionDialog(
    std::span<const core::TimelineEventField> projection, QWidget* parent)
    : QDialog{parent} {
  setWindowTitle(tr("Export Columns"));
  setModal(true);
  resize(560, 620);
  auto* layout = new QVBoxLayout{this};
  projection_ = new EventProjectionWidget{projection, this};
  layout->addWidget(projection_, 1);
  auto* buttons = new QDialogButtonBox{this};
  save_ = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
  auto* cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);

  SetAutomationIdentifier(*this, u"eventProjectionDialog");
  SetAutomationIdentifier(*save_, u"saveProjectionButton");
  SetAutomationIdentifier(*cancel, u"cancelProjectionButton");

  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(projection_, &EventProjectionWidget::validityChanged, save_,
          &QPushButton::setEnabled);
  save_->setEnabled(!projection_->Projection().empty());
  layout->addWidget(buttons);
}

std::vector<core::TimelineEventField> EventProjectionDialog::Projection(
    void) const {
  return projection_->Projection();
}

}  // namespace edit_atlas::frontends::widgets
