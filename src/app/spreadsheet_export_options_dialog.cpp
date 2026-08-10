#include <edit_atlas/app/spreadsheet_export_options_dialog.hpp>

#include "accessibility.hpp"
#include "event_projection_widget.hpp"

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <span>
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

} // namespace

SpreadsheetExportOptionsDialog::SpreadsheetExportOptionsDialog(
    ApplicationLanguage application_language,
    std::span<const core::TimelineEventField> projection, QWidget *parent)
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

    projection_ = new EventProjectionWidget{projection, this};
    layout->addWidget(projection_, 1);

    auto *buttons = new QDialogButtonBox{this};
    continue_ =
        buttons->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    auto *cancel =
        buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);

    SetAutomationIdentifier(*this, u"spreadsheetOptionsDialog");
    SetAutomationIdentifier(*workbook_language_, u"workbookLanguageSelector");
    SetAutomationIdentifier(*timeline_, u"includeTimelineSheetCheckBox");
    SetAutomationIdentifier(*diagnostics_, u"includeDiagnosticsSheetCheckBox");
    SetAutomationIdentifier(*continue_, u"continueSpreadsheetExportButton");
    SetAutomationIdentifier(*cancel, u"cancelSpreadsheetExportButton");

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(projection_, &EventProjectionWidget::ValidityChanged, continue_,
            &QPushButton::setEnabled);
    continue_->setEnabled(!projection_->Projection().empty());
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
    return projection_->Projection();
}

} // namespace edit_atlas::app
