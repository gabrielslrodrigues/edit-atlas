#include <edit_atlas/app/spreadsheet_export_options_dialog.hpp>

#include "accessibility.hpp"
#include "event_projection_widget.hpp"

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace edit_atlas::app {
namespace {

[[nodiscard]] formats::xlsx::WorkbookLanguage
WorkbookLanguageFor(presentation::ApplicationLanguage language) noexcept {
    switch (language) {
    case presentation::ApplicationLanguage::kEnglish:
        return formats::xlsx::WorkbookLanguage::kEnglish;
    case presentation::ApplicationLanguage::kBrazilianPortuguese:
        return formats::xlsx::WorkbookLanguage::kBrazilianPortuguese;
    }
    return formats::xlsx::WorkbookLanguage::kEnglish;
}

} // namespace

SpreadsheetExportOptionsDialog::SpreadsheetExportOptionsDialog(
    presentation::ApplicationLanguage application_language,
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

    video_group_ = new QGroupBox{tr("Rendered video"), this};
    auto *video_layout = new QVBoxLayout{video_group_};
    auto *video_requirement = new QLabel{
        tr("Initial-frame images require a constant-frame-rate MOV, MP4, or "
           "MXF render with embedded starting timecode, frame rate, and "
           "duration matching the imported EDL."),
        video_group_};
    video_requirement->setWordWrap(true);
    video_layout->addWidget(video_requirement);
    auto *video_path_layout = new QHBoxLayout;
    video_path_ = new QLineEdit{video_group_};
    video_path_->setPlaceholderText(tr("Select the matching rendered video"));
    video_path_->setAccessibleName(tr("Rendered video path"));
    auto *browse_video = new QPushButton{tr("Browse…"), video_group_};
    video_path_layout->addWidget(video_path_, 1);
    video_path_layout->addWidget(browse_video);
    video_layout->addLayout(video_path_layout);
    layout->addWidget(video_group_);

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
    SetAutomationIdentifier(*video_group_, u"renderedVideoGroup");
    SetAutomationIdentifier(*video_path_, u"renderedVideoPathField");
    SetAutomationIdentifier(*browse_video, u"browseRenderedVideoButton");
    SetAutomationIdentifier(*continue_, u"continueSpreadsheetExportButton");
    SetAutomationIdentifier(*cancel, u"cancelSpreadsheetExportButton");

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(projection_, &EventProjectionWidget::ProjectionChanged, this,
            &SpreadsheetExportOptionsDialog::UpdateControls);
    connect(video_path_, &QLineEdit::textChanged, this,
            [this](const QString &) { UpdateControls(); });
    connect(browse_video, &QPushButton::clicked, this,
            &SpreadsheetExportOptionsDialog::BrowseForVideo);
    UpdateControls();
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

QString SpreadsheetExportOptionsDialog::VideoPath(void) const {
    return video_group_->isHidden() ? QString{} : video_path_->text().trimmed();
}

void SpreadsheetExportOptionsDialog::BrowseForVideo(void) {
    QFileDialog dialog{this, tr("Select Rendered Video")};
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(
        {tr("Supported video files (*.mov *.mp4 *.mxf)"), tr("All files (*)")});
    dialog.resize(1000, 650);
    SetAutomationIdentifier(dialog, u"renderedVideoOpenFileDialog");
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto files = dialog.selectedFiles();
    if (!files.empty()) {
        video_path_->setText(files.front());
    }
}

void SpreadsheetExportOptionsDialog::UpdateControls(void) {
    const auto projection = projection_->Projection();
    const auto requires_video = std::ranges::contains(
        projection, core::TimelineEventField::kInitialFrame);
    video_group_->setVisible(requires_video);
    continue_->setEnabled(!projection.empty() &&
                          (!requires_video || !VideoPath().isEmpty()));
}

} // namespace edit_atlas::app
