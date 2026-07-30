#include <edit_atlas/app/spreadsheet_export_options_dialog.hpp>

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
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

} // namespace

SpreadsheetExportOptionsDialog::SpreadsheetExportOptionsDialog(
    ApplicationLanguage application_language, QWidget *parent)
    : QDialog{parent} {
    setWindowTitle(tr("Spreadsheet Options"));
    setModal(true);

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

    auto *buttons = new QDialogButtonBox{this};
    buttons->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
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

} // namespace edit_atlas::app
