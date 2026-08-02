#include <edit_atlas/app/spreadsheet_export_options_dialog.hpp>

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>

#include <string>
#include <vector>

namespace edit_atlas::app {
namespace {

[[nodiscard]] formats::xlsx::WorkbookLanguage
WorkbookLanguageFor(ApplicationLanguage language) noexcept {
    switch (language) {
    case ApplicationLanguage::kEnglish:
        return formats::xlsx::WorkbookLanguage::kEnglish;
    case ApplicationLanguage::kBrazilianPortuguese:
        return formats::xlsx::WorkbookLanguage::kBrazilianPortuguese;
    }
    return formats::xlsx::WorkbookLanguage::kEnglish;
}

[[nodiscard]] QString ColumnText(core::TimelineEventField field) {
    switch (field) {
    case core::TimelineEventField::kEventIdentifier:
        return SpreadsheetExportOptionsDialog::tr("Event");
    case core::TimelineEventField::kReel:
        return SpreadsheetExportOptionsDialog::tr("Reel");
    case core::TimelineEventField::kTrackKind:
        return SpreadsheetExportOptionsDialog::tr("Track type");
    case core::TimelineEventField::kTrackIdentifier:
        return SpreadsheetExportOptionsDialog::tr("Track");
    case core::TimelineEventField::kEditType:
        return SpreadsheetExportOptionsDialog::tr("Edit type");
    case core::TimelineEventField::kTransitionIdentifier:
        return SpreadsheetExportOptionsDialog::tr("Transition");
    case core::TimelineEventField::kTransitionDuration:
        return SpreadsheetExportOptionsDialog::tr("Transition frames");
    case core::TimelineEventField::kSourceIn:
        return SpreadsheetExportOptionsDialog::tr("Source in");
    case core::TimelineEventField::kSourceOut:
        return SpreadsheetExportOptionsDialog::tr("Source out");
    case core::TimelineEventField::kRecordIn:
        return SpreadsheetExportOptionsDialog::tr("Record in");
    case core::TimelineEventField::kRecordOut:
        return SpreadsheetExportOptionsDialog::tr("Record out");
    case core::TimelineEventField::kDuration:
        return SpreadsheetExportOptionsDialog::tr("Duration frames");
    case core::TimelineEventField::kClipName:
        return SpreadsheetExportOptionsDialog::tr("Clip name");
    case core::TimelineEventField::kSourceFile:
        return SpreadsheetExportOptionsDialog::tr("Source file");
    case core::TimelineEventField::kComments:
        return SpreadsheetExportOptionsDialog::tr("Comments");
    case core::TimelineEventField::kSourceLine:
        return SpreadsheetExportOptionsDialog::tr("Source line");
    }
    return {};
}

} // namespace

SpreadsheetExportOptionsDialog::SpreadsheetExportOptionsDialog(
    ApplicationLanguage application_language, QWidget *parent)
    : QDialog{parent} {
    setWindowTitle(tr("Spreadsheet Options"));
    setModal(true);
    resize(620, 700);

    auto *layout = new QVBoxLayout{this};
    auto *description = new QLabel{
        tr("Choose the information to include in the workbook."), this};
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *language_label = new QLabel{tr("Workbook language"), this};
    layout->addWidget(language_label);
    workbook_language_ = new QComboBox{this};
    workbook_language_->addItem(
        tr("Same as application"),
        static_cast<int>(WorkbookLanguageFor(application_language)));
    workbook_language_->addItem(
        tr("English"),
        static_cast<int>(formats::xlsx::WorkbookLanguage::kEnglish));
    workbook_language_->addItem(
        tr("Português (Brasil)"),
        static_cast<int>(
            formats::xlsx::WorkbookLanguage::kBrazilianPortuguese));
    workbook_language_->setAccessibleName(tr("Workbook language"));
    layout->addWidget(workbook_language_);

    timeline_ = new QCheckBox{tr("Include timeline summary"), this};
    timeline_->setChecked(true);
    layout->addWidget(timeline_);

    diagnostics_ = new QCheckBox{tr("Include diagnostics"), this};
    diagnostics_->setChecked(true);
    layout->addWidget(diagnostics_);

    auto *columns_label = new QLabel{tr("Event columns"), this};
    layout->addWidget(columns_label);
    auto *columns_description = new QLabel{
        tr("Select the columns to export and arrange them in output order."),
        this};
    columns_description->setWordWrap(true);
    layout->addWidget(columns_description);

    event_columns_ = new QListWidget{this};
    event_columns_->setObjectName(QStringLiteral("eventColumnsList"));
    event_columns_->setAccessibleName(tr("Event columns"));
    for (const auto field : core::DefaultTimelineEventProjection()) {
        auto *item = new QListWidgetItem{ColumnText(field), event_columns_};
        item->setData(Qt::UserRole, static_cast<int>(field));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
    event_columns_->setCurrentRow(0);
    layout->addWidget(event_columns_, 1);

    auto *column_buttons = new QHBoxLayout;
    move_up_ = new QPushButton{tr("Move up"), this};
    move_down_ = new QPushButton{tr("Move down"), this};
    column_buttons->addWidget(move_up_);
    column_buttons->addWidget(move_down_);
    column_buttons->addStretch();
    layout->addLayout(column_buttons);

    column_error_ = new QLabel{tr("Select at least one event column."), this};
    column_error_->setObjectName(QStringLiteral("columnSelectionErrorLabel"));
    layout->addWidget(column_error_);

    auto *buttons = new QDialogButtonBox{this};
    continue_ =
        buttons->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(move_up_, &QPushButton::clicked, this,
            [this] { MoveCurrentColumn(-1); });
    connect(move_down_, &QPushButton::clicked, this,
            [this] { MoveCurrentColumn(1); });
    connect(event_columns_, &QListWidget::currentRowChanged, this,
            [this] { UpdateColumnControls(); });
    connect(event_columns_, &QListWidget::itemChanged, this,
            [this] { UpdateColumnControls(); });
    UpdateColumnControls();
}

std::vector<core::MetadataEntry>
SpreadsheetExportOptionsDialog::Options(void) const {
    const auto language = static_cast<formats::xlsx::WorkbookLanguage>(
        workbook_language_->currentData().toInt());
    return {
        core::MetadataEntry{
            .key = std::string{formats::xlsx::kWorkbookLanguageOption},
            .value =
                std::string{
                    formats::xlsx::WorkbookLanguageTag(language),
                },
        },
        core::MetadataEntry{
            .key =
                std::string{
                    formats::xlsx::kIncludeTimelineSheetOption,
                },
            .value = timeline_->isChecked(),
        },
        core::MetadataEntry{
            .key =
                std::string{
                    formats::xlsx::kIncludeDiagnosticsSheetOption,
                },
            .value = diagnostics_->isChecked(),
        },
    };
}

std::vector<core::TimelineEventField>
SpreadsheetExportOptionsDialog::EventProjection(void) const {
    std::vector<core::TimelineEventField> projection;
    projection.reserve(static_cast<std::size_t>(event_columns_->count()));
    for (int row = 0; row < event_columns_->count(); ++row) {
        const auto *item = event_columns_->item(row);
        if (item->checkState() == Qt::Checked) {
            projection.push_back(static_cast<core::TimelineEventField>(
                item->data(Qt::UserRole).toInt()));
        }
    }
    return projection;
}

void SpreadsheetExportOptionsDialog::MoveCurrentColumn(int offset) {
    const auto current_row = event_columns_->currentRow();
    const auto target_row = current_row + offset;
    if (current_row < 0 || target_row < 0 ||
        target_row >= event_columns_->count()) {
        return;
    }
    auto *item = event_columns_->takeItem(current_row);
    event_columns_->insertItem(target_row, item);
    event_columns_->setCurrentRow(target_row);
    UpdateColumnControls();
}

void SpreadsheetExportOptionsDialog::UpdateColumnControls(void) {
    const auto current_row = event_columns_->currentRow();
    move_up_->setEnabled(current_row > 0);
    move_down_->setEnabled(current_row >= 0 &&
                           current_row + 1 < event_columns_->count());
    const auto valid = !EventProjection().empty();
    continue_->setEnabled(valid);
    column_error_->setVisible(!valid);
}

} // namespace edit_atlas::app
