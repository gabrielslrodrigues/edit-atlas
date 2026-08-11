#include <edit_atlas/app/timeline_document_controller.hpp>

#include "accessibility.hpp"

#include <edit_atlas/app/application_menu_bar.hpp>
#include <edit_atlas/app/desktop_integration.hpp>
#include <edit_atlas/app/diagnostic_text.hpp>
#include <edit_atlas/app/spreadsheet_export_options_dialog.hpp>
#include <edit_atlas/app/timeline_document_view.hpp>
#include <edit_atlas/app/timeline_document_workflow.hpp>

#include "timeline_template_controller.hpp"

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_frame_extraction_service.hpp>
#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>

#include <QChar>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <Qt>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::app {
namespace {

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] std::filesystem::path FilesystemPath(const QString &path) {
    const auto utf8 = path.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

[[nodiscard]] QString PathText(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return QString::fromUtf8(reinterpret_cast<const char *>(utf8.data()),
                             static_cast<qsizetype>(utf8.size()));
}

[[nodiscard]] QString TimecodeText(const core::Timecode &timecode) {
    const auto separator = timecode.mode() == core::TimecodeMode::kDropFrame
                               ? QChar{u';'}
                               : QChar{u':'};
    return QStringLiteral("%1:%2:%3%4%5")
        .arg(timecode.hours(), 2, 10, QChar{u'0'})
        .arg(timecode.minutes(), 2, 10, QChar{u'0'})
        .arg(timecode.seconds(), 2, 10, QChar{u'0'})
        .arg(separator)
        .arg(timecode.frames(), 2, 10, QChar{u'0'});
}

[[nodiscard]] bool
HasDiagnosticCode(const std::vector<core::Diagnostic> &diagnostics,
                  std::string_view code) {
    return std::ranges::any_of(diagnostics,
                               [code](const core::Diagnostic &diagnostic) {
                                   return diagnostic.code == code;
                               });
}

} // namespace

TimelineDocumentController::TimelineDocumentController(
    const core::FormatRegistry &registry, ApplicationMenuBar &menu_bar,
    TimelineDocumentView &view, ApplicationLanguage language, QWidget &window)
    : QObject{&window}, registry_{registry}, menu_bar_{menu_bar}, view_{view},
      language_{language}, window_{window} {
    workflow_ = new TimelineDocumentWorkflow{registry_, this};
    template_controller_ = new TimelineTemplateController{view_, window_, this};
    connect(workflow_, &TimelineDocumentWorkflow::ImportFinished, this,
            &TimelineDocumentController::HandleImportFinished);
    connect(workflow_, &TimelineDocumentWorkflow::RenderedVideoExportFinished,
            this,
            &TimelineDocumentController::HandleRenderedVideoExportFinished);
    connect(workflow_,
            &TimelineDocumentWorkflow::FrameExtractionProgressChanged, this,
            [this](qulonglong completed, qulonglong total) {
                if (export_progress_ == nullptr) {
                    return;
                }
                export_progress_->setRange(0, static_cast<int>(total));
                export_progress_->setValue(static_cast<int>(completed));
                export_progress_->setLabelText(
                    tr("Extracting initial frames: %1 of %2")
                        .arg(completed)
                        .arg(total));
            });
    connect(&menu_bar_, &ApplicationMenuBar::OpenRequested, this,
            [this](void) { OpenTimeline(); });
    connect(&menu_bar_, &ApplicationMenuBar::OpenPathRequested, this,
            [this](const QString &path) { OpenTimeline(path); });
    connect(&menu_bar_, &ApplicationMenuBar::ExportSpreadsheetRequested, this,
            &TimelineDocumentController::ExportSpreadsheet);
    connect(&view_, &TimelineDocumentView::OpenRequested, this,
            [this](void) { OpenTimeline(); });
    connect(&view_, &TimelineDocumentView::ExportRequested, this,
            &TimelineDocumentController::ExportSpreadsheet);
    connect(&view_, &TimelineDocumentView::FilterChanged, this,
            &TimelineDocumentController::ApplyFilter);
}

void TimelineDocumentController::ApplyFilter(void) {
    filter_query_ = view_.FilterQuery();
    if (!timeline_.has_value()) {
        event_selection_.clear();
        filter_valid_ = true;
        view_.SetFilterError({});
        menu_bar_.SetExportAvailable(true);
        template_controller_->SetFilterState(filter_query_, true);
        return;
    }
    auto result = services::FilterTimelineEvents(*timeline_, filter_query_);
    if (!result.has_value()) {
        event_selection_.clear();
        filter_valid_ = false;
        view_.SetEventSelection(event_selection_);
        view_.SetFilterError(
            tr("Condition %1 has an invalid regular expression: %2")
                .arg(
                    static_cast<qulonglong>(result.error().condition_index + 1))
                .arg(Utf8(result.error().message)));
        menu_bar_.SetExportAvailable(false);
        template_controller_->SetFilterState(filter_query_, false);
        return;
    }
    event_selection_ = std::move(*result);
    filter_valid_ = true;
    view_.SetFilterError({});
    view_.SetEventSelection(event_selection_);
    menu_bar_.SetExportAvailable(true);
    template_controller_->SetFilterState(filter_query_, true);
}

void TimelineDocumentController::ExportSpreadsheet(void) {
    if (!interactions_enabled_ || !timeline_.has_value() || !filter_valid_ ||
        IsBusy()) {
        return;
    }

    const auto exporters = registry_.ExportersForExtension("xlsx");
    if (exporters.empty()) {
        QMessageBox message{
            QMessageBox::Critical,
            tr("Could not export spreadsheet"),
            tr("No registered exporter can create an Excel workbook."),
            QMessageBox::NoButton,
            &window_,
        };
        auto *close = message.addButton(tr("Close"), QMessageBox::RejectRole);

        SetAutomationIdentifier(message,
                                u"spreadsheetExporterUnavailableDialog");
        SetAutomationIdentifier(*close, u"closeDialogButton");

        message.exec();
        return;
    }

    SpreadsheetExportOptionsDialog options_dialog{
        language_, template_controller_->EventProjection(), &window_};
    if (options_dialog.exec() != QDialog::Accepted) {
        return;
    }
    auto options = options_dialog.Options();
    const auto video_path = options_dialog.VideoPath();
    template_controller_->SetEventProjection(options_dialog.EventProjection());

    const QFileInfo source{current_path_};
    auto suggested_name = source.completeBaseName();
    if (suggested_name.isEmpty()) {
        suggested_name = tr("timeline");
    }
    suggested_name += QStringLiteral("-report.xlsx");

    const QSettings settings;
    auto directory =
        settings.value(QStringLiteral("export/lastDirectory")).toString();
    if (directory.isEmpty()) {
        directory = source.absolutePath();
    }

    QFileDialog dialog{
        &window_,
        tr("Export Spreadsheet"),
        QDir{directory}.filePath(suggested_name),
    };
    SetAutomationIdentifier(dialog, u"spreadsheetSaveFileDialog");
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setDefaultSuffix(QStringLiteral("xlsx"));
    dialog.setNameFilters({tr("Excel workbook (*.xlsx)"), tr("All files (*)")});
    dialog.setOption(QFileDialog::DontConfirmOverwrite, true);
    dialog.resize(1000, 650);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto selected_files = dialog.selectedFiles();
    if (selected_files.empty()) {
        return;
    }

    const auto destination = selected_files.front();
    const auto replace_existing = QFileInfo::exists(destination);
    if (replace_existing) {
        QMessageBox confirmation{
            QMessageBox::Question,
            tr("Replace Existing File?"),
            tr("%1 already exists. Do you want to replace it?")
                .arg(QFileInfo{destination}.fileName()),
            QMessageBox::NoButton,
            &window_,
        };
        auto *replace_button =
            confirmation.addButton(tr("Replace"), QMessageBox::AcceptRole);
        auto *cancel_button =
            confirmation.addButton(tr("Cancel"), QMessageBox::RejectRole);

        SetAutomationIdentifier(confirmation, u"replaceSpreadsheetDialog");
        SetAutomationIdentifier(*replace_button, u"replaceSpreadsheetButton");
        SetAutomationIdentifier(*cancel_button,
                                u"cancelReplaceSpreadsheetButton");

        confirmation.exec();
        if (confirmation.clickedButton() != replace_button) {
            return;
        }
    }

    QSettings writable_settings;
    writable_settings.setValue(QStringLiteral("export/lastDirectory"),
                               QFileInfo{destination}.absolutePath());

    services::TimelineDocumentExportRequest request{
        .path = FilesystemPath(destination),
        .format_identifier = exporters.front()->descriptor().identifier,
        .timeline =
            services::SelectTimelineEvents(*timeline_, event_selection_),
        .event_projection =
            std::vector<core::TimelineEventField>{
                template_controller_->EventProjection().begin(),
                template_controller_->EventProjection().end()},
        .options = std::move(options),
        .event_images = {},
        .replace_existing = replace_existing,
    };
    SPDLOG_INFO("Spreadsheet export started with {} of {} event(s)",
                event_selection_.size(), timeline_->events.size());
    emit BusyChanged(true);
    emit StatusMessageChanged(
        tr("Exporting %1…").arg(QFileInfo{destination}.fileName()));
    if (!video_path.isEmpty()) {
        export_progress_ = new QProgressDialog{tr("Validating rendered video…"),
                                               tr("Cancel"), 0, 0, &window_};
        export_progress_->setWindowTitle(tr("Export Spreadsheet"));
        export_progress_->setWindowModality(Qt::WindowModal);
        export_progress_->setAutoClose(false);
        export_progress_->setAutoReset(false);
        SetAutomationIdentifier(*export_progress_,
                                u"spreadsheetExportProgressDialog");
        if (auto *cancel = export_progress_->findChild<QPushButton *>();
            cancel != nullptr) {
            SetAutomationIdentifier(*cancel, u"cancelFrameExtractionButton");
        }
        connect(export_progress_, &QProgressDialog::canceled, workflow_,
                &TimelineDocumentWorkflow::CancelRenderedVideoExport);
        export_progress_->show();
    }
    workflow_->ExportWithRenderedVideo(
        services::TimelineRenderedVideoExportRequest{
            .document_export = std::move(request),
            .reference_timeline = *timeline_,
            .video_path =
                video_path.isEmpty()
                    ? std::optional<std::filesystem::path>{}
                    : std::optional<std::filesystem::path>{FilesystemPath(
                          video_path)},
        });
}

bool TimelineDocumentController::IsBusy(void) const noexcept {
    return workflow_->IsBusy();
}

void TimelineDocumentController::OpenTimeline(void) {
    if (!interactions_enabled_ || IsBusy()) {
        return;
    }

    QStringList patterns;
    for (const auto &format : registry_.importer_formats()) {
        for (const auto &extension : format.extensions) {
            patterns.emplace_back(QStringLiteral("*.%1").arg(Utf8(extension)));
        }
    }
    patterns.removeDuplicates();
    QFileDialog dialog{&window_, tr("Open Timeline")};
    SetAutomationIdentifier(dialog, u"timelineOpenFileDialog");
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters({tr("Supported timeline files (%1)")
                               .arg(patterns.isEmpty() ? QStringLiteral("*")
                                                       : patterns.join(u' ')),
                           tr("All files (*)")});
    dialog.resize(1000, 650);
    if (dialog.exec() == QDialog::Accepted) {
        const auto selected_files = dialog.selectedFiles();
        if (!selected_files.empty()) {
            OpenTimeline(selected_files.front());
        }
    }
}

void TimelineDocumentController::OpenTimeline(const QString &path) {
    if (!interactions_enabled_ || path.isEmpty() || IsBusy()) {
        return;
    }
    StartImport(path);
}

void TimelineDocumentController::SetInteractionsEnabled(bool enabled) {
    interactions_enabled_ = enabled;
}

void TimelineDocumentController::SetLanguage(ApplicationLanguage language) {
    language_ = language;
    ApplyFilter();
    if (workflow_->IsExporting()) {
        emit StatusMessageChanged(tr("Exporting spreadsheet…"));
    }
}

void TimelineDocumentController::ClearTimeline(void) {
    menu_bar_.SetDocumentAvailable(false);
    menu_bar_.SetExportAvailable(true);
    timeline_.reset();
    event_selection_.clear();
    filter_valid_ = true;
    current_path_.clear();
    requested_frame_rate_.reset();
    view_.Clear();
    filter_query_ = view_.FilterQuery();
    template_controller_->RestoreForTimeline();
}

void TimelineDocumentController::HandleRenderedVideoExportFinished(void) {
    emit BusyChanged(false);
    emit StatusMessageCleared();
    if (export_progress_ != nullptr) {
        export_progress_->close();
        export_progress_->deleteLater();
        export_progress_ = nullptr;
    }

    auto result = workflow_->RenderedVideoExportResult();
    if (!result.has_value()) {
        SPDLOG_ERROR("Rendered-video spreadsheet export failed at stage {} "
                     "with {} "
                     "diagnostic(s)",
                     static_cast<int>(result.error().kind),
                     result.error().diagnostics.size());
        if (!HasDiagnosticCode(
                result.error().diagnostics,
                services::timeline_frame_extraction_diagnostic_code::
                    kCancelled)) {
            ShowRenderedVideoExportFailure(result.error());
        }
        return;
    }
    SPDLOG_INFO("Spreadsheet export completed with {} diagnostic(s)",
                result->document_export.diagnostics.size());

    const auto path = PathText(result->document_export.path);
    QMessageBox message{&window_};
    message.setWindowTitle(result->document_export.diagnostics.empty()
                               ? tr("Spreadsheet Exported")
                               : tr("Spreadsheet Exported with Warnings"));
    message.setIcon(result->document_export.diagnostics.empty()
                        ? QMessageBox::Information
                        : QMessageBox::Warning);
    message.setText(tr("The spreadsheet was saved to:\n%1").arg(path));
    QStringList informative_text;
    if (result->video_information.has_value() &&
        result->video_mapping.has_value()) {
        informative_text.emplace_back(
            tr("Embedded initial-frame images from %1 (%2, %3×%4, %5/%6 "
               "fps, starting at %7); decoded %8 unique frame(s).")
                .arg(QFileInfo{PathText(result->video_information->path)}
                         .fileName())
                .arg(Utf8(result->video_information->container_long_name))
                .arg(result->video_information
                         ->streams[result->video_information
                                       ->selected_video_stream]
                         .width)
                .arg(result->video_information
                         ->streams[result->video_information
                                       ->selected_video_stream]
                         .height)
                .arg(result->video_mapping->video_start_timecode.rate()
                         .numerator())
                .arg(result->video_mapping->video_start_timecode.rate()
                         .denominator())
                .arg(TimecodeText(result->video_mapping->video_start_timecode))
                .arg(static_cast<qulonglong>(result->unique_frame_count)));
    }
    if (!result->document_export.diagnostics.empty()) {
        informative_text.emplace_back(
            tr("The workbook was created, but the exporter reported "
               "warnings."));
        message.setDetailedText(
            diagnostic_text::Summary(result->document_export.diagnostics));
    }
    if (!informative_text.empty()) {
        message.setInformativeText(informative_text.join(u'\n'));
    }
    auto *reveal_button =
        message.addButton(tr("Reveal File"), QMessageBox::ActionRole);
    auto *close_button =
        message.addButton(tr("Close"), QMessageBox::RejectRole);

    SetAutomationIdentifier(message, u"spreadsheetExportResultDialog");
    SetAutomationIdentifier(*reveal_button, u"revealSpreadsheetButton");
    SetAutomationIdentifier(*close_button, u"closeDialogButton");

    message.exec();
    if (message.clickedButton() == reveal_button &&
        !desktop_integration::RevealFile(path)) {
        QMessageBox warning{
            QMessageBox::Warning,
            tr("Could Not Reveal File"),
            tr("The spreadsheet was saved, but its location could not be "
               "opened."),
            QMessageBox::NoButton,
            &window_,
        };
        auto *close = warning.addButton(tr("Close"), QMessageBox::RejectRole);

        SetAutomationIdentifier(warning, u"revealSpreadsheetFailureDialog");
        SetAutomationIdentifier(*close, u"closeDialogButton");

        warning.exec();
    }
}

void TimelineDocumentController::HandleImportFinished(void) {
    emit BusyChanged(false);
    auto result = workflow_->ImportResult();

    if (!result.has_value() &&
        result.error().kind ==
            services::TimelineDocumentImportFailureKind::kImportFailed &&
        !requested_frame_rate_.has_value() &&
        HasDiagnosticCode(
            result.error().diagnostics,
            formats::cmx3600::diagnostic_code::kMissingFrameRate)) {
        const QStringList frame_rate_labels{
            tr("23.976 fps"), tr("24 fps"), tr("25 fps"),    tr("29.97 fps"),
            tr("30 fps"),     tr("50 fps"), tr("59.94 fps"), tr("60 fps"),
        };
        constexpr std::array<std::string_view, 8> kFrameRates{
            "24000/1001", "24", "25",         "30000/1001",
            "30",         "50", "60000/1001", "60",
        };
        QInputDialog dialog{&window_};
        dialog.setWindowTitle(tr("Select Frame Rate"));
        dialog.setLabelText(
            tr("This non-drop-frame EDL does not declare its frame rate."));
        dialog.setComboBoxItems(frame_rate_labels);
        dialog.setComboBoxEditable(false);
        dialog.setTextValue(frame_rate_labels.at(1));
        if (auto *selector = dialog.findChild<QComboBox *>();
            selector != nullptr) {
            SetAutomationIdentifier(*selector, u"frameRateSelector");
        }
        SetInputDialogButtonAutomationIdentifiers(
            dialog, u"acceptFrameRateButton", u"cancelFrameRateButton");
        SetAutomationIdentifier(dialog, u"frameRateDialog");
        if (dialog.exec() == QDialog::Accepted) {
            const auto selected = dialog.textValue();
            const auto index = frame_rate_labels.indexOf(selected);
            if (index >= 0) {
                StartImport(
                    PathText(result.error().path),
                    std::string{kFrameRates[static_cast<std::size_t>(index)]});
                return;
            }
        }
    }

    if (!result.has_value()) {
        SPDLOG_ERROR("Timeline import failed at stage {} with {} diagnostic(s)",
                     static_cast<int>(result.error().kind),
                     result.error().diagnostics.size());
        current_path_ = PathText(result.error().path);
        view_.ShowImportFailure(result.error());
        return;
    }

    timeline_ = std::move(result->timeline);
    auto diagnostics = std::move(result->diagnostics);
    view_.ShowTimeline(*timeline_, QFileInfo{current_path_}.fileName(),
                       diagnostics);
    ApplyFilter();
    menu_bar_.SetDocumentAvailable(true);
    SPDLOG_INFO("Timeline import completed with {} event(s) and {} "
                "diagnostic(s)",
                timeline_->events.size(), diagnostics.size());
    menu_bar_.RememberRecentFile(PathText(result->path));
}

void TimelineDocumentController::ShowExportFailure(
    const services::TimelineDocumentExportFailure &failure) {
    QString description;
    switch (failure.kind) {
    case services::TimelineDocumentExportFailureKind::kInvalidRequest:
        description = tr("Select at least one unique event column.");
        break;
    case services::TimelineDocumentExportFailureKind::kExportFailed:
        description = tr("The exporter could not create the spreadsheet.");
        break;
    case services::TimelineDocumentExportFailureKind::kDestinationExists:
        description =
            tr("The destination file already exists and was not replaced.");
        break;
    case services::TimelineDocumentExportFailureKind::kWriteFailed:
        description = tr("The spreadsheet could not be written: %1")
                          .arg(Utf8(failure.filesystem_error.message()));
        break;
    case services::TimelineDocumentExportFailureKind::kCommitFailed:
        description =
            tr("The completed spreadsheet could not replace the destination "
               "file: %1")
                .arg(Utf8(failure.filesystem_error.message()));
        break;
    }

    QMessageBox message{
        QMessageBox::Critical,
        tr("Could not export spreadsheet"),
        description,
        QMessageBox::NoButton,
        &window_,
    };
    if (!failure.diagnostics.empty()) {
        message.setDetailedText(diagnostic_text::Summary(failure.diagnostics));
    }
    auto *close = message.addButton(tr("Close"), QMessageBox::RejectRole);

    SetAutomationIdentifier(message, u"spreadsheetExportFailureDialog");
    SetAutomationIdentifier(*close, u"closeDialogButton");

    message.exec();
}

void TimelineDocumentController::ShowRenderedVideoExportFailure(
    const services::TimelineRenderedVideoExportFailure &failure) {
    if (failure.document_export_failure.has_value()) {
        ShowExportFailure(*failure.document_export_failure);
        return;
    }

    QString description;
    switch (failure.kind) {
    case services::TimelineRenderedVideoExportFailureKind::kVideoRequired:
        description = tr("Select a matching rendered video for the Initial "
                         "Frame column.");
        break;
    case services::TimelineRenderedVideoExportFailureKind::
        kVideoInspectionFailed:
        description =
            tr("The rendered video could not be validated. It must be a "
               "constant-frame-rate MOV, MP4, or MXF file with embedded "
               "starting timecode, frame rate, and duration matching the "
               "imported EDL.");
        break;
    case services::TimelineRenderedVideoExportFailureKind::
        kFrameExtractionFailed:
        description =
            tr("Initial frames could not be extracted from the rendered "
               "video.");
        break;
    case services::TimelineRenderedVideoExportFailureKind::
        kDocumentExportFailed:
        description = tr("The prepared spreadsheet could not be exported.");
        break;
    }

    QMessageBox message{
        QMessageBox::Critical,
        tr("Could not use rendered video"),
        description,
        QMessageBox::NoButton,
        &window_,
    };
    if (!failure.diagnostics.empty()) {
        message.setDetailedText(diagnostic_text::Summary(failure.diagnostics));
    }
    auto *close = message.addButton(tr("Close"), QMessageBox::RejectRole);
    SetAutomationIdentifier(message, u"renderedVideoExportFailureDialog");
    SetAutomationIdentifier(*close, u"closeDialogButton");
    message.exec();
}

void TimelineDocumentController::StartImport(
    const QString &path, std::optional<std::string> frame_rate) {
    ClearTimeline();
    current_path_ = path;
    requested_frame_rate_ = frame_rate;
    emit BusyChanged(true);
    view_.ShowLoading(QFileInfo{path}.fileName());

    services::TimelineDocumentImportRequest request{
        .path = FilesystemPath(path),
        .format_identifier = {},
        .options = {},
    };
    if (frame_rate.has_value()) {
        request.options.emplace_back(core::MetadataEntry{
            .key = std::string{formats::cmx3600::kFrameRateOption},
            .value = *frame_rate,
        });
    }
    SPDLOG_INFO("Timeline import started");
    workflow_->Import(std::move(request));
}

} // namespace edit_atlas::app
