#include <edit_atlas/frontends/quick/spreadsheet_export_view_model.hpp>

#include <edit_atlas/presentation/desktop_integration.hpp>
#include <edit_atlas/presentation/diagnostic_text.hpp>
#include <edit_atlas/presentation/timeline_document_view_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/services/timeline_frame_extraction_service.hpp>
#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>

#include <QChar>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace edit_atlas::frontends::quick {
namespace {

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

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
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

[[nodiscard]] QString RenderedVideoFailureText(
    services::TimelineRenderedVideoExportFailureKind kind) {
    using services::TimelineRenderedVideoExportFailureKind;
    switch (kind) {
    case TimelineRenderedVideoExportFailureKind::kVideoRequired:
        return SpreadsheetExportViewModel::tr(
            "Select a matching rendered video for the Initial Frame column.");
    case TimelineRenderedVideoExportFailureKind::kVideoInspectionFailed:
        return SpreadsheetExportViewModel::tr(
            "The rendered video could not be validated. It must be a "
            "constant-frame-rate MOV, MP4, or MXF file with embedded starting "
            "timecode, frame rate, and duration matching the imported EDL.");
    case TimelineRenderedVideoExportFailureKind::kFrameExtractionFailed:
        return SpreadsheetExportViewModel::tr(
            "Initial frames could not be extracted from the rendered video.");
    case TimelineRenderedVideoExportFailureKind::kDocumentExportFailed:
        return SpreadsheetExportViewModel::tr(
            "The prepared spreadsheet could not be exported.");
    }
    return {};
}

[[nodiscard]] bool
HasDiagnosticCode(std::span<const core::Diagnostic> diagnostics,
                  std::string_view code) {
    return std::ranges::any_of(diagnostics,
                               [code](const core::Diagnostic &diagnostic) {
                                   return diagnostic.code == code;
                               });
}

} // namespace

SpreadsheetExportViewModel::SpreadsheetExportViewModel(
    const core::FormatRegistry &registry,
    presentation::TimelineDocumentViewModel &document_view_model,
    QObject *parent)
    : QObject{parent}, document_view_model_{document_view_model} {
    const auto exporters = registry.ExportersForExtension("xlsx");
    if (!exporters.empty()) {
        exporter_identifier_ = exporters.front()->descriptor().identifier;
    }
    connect(&document_view_model_,
            &presentation::TimelineDocumentViewModel::DocumentStateChanged,
            this, [this](void) {
                if (document_view_model_.DocumentState() ==
                    presentation::TimelineDocumentState::kImporting) {
                    ClearResult();
                }
                emit availabilityChanged();
                emit suggestedDestinationChanged();
            });
    connect(&document_view_model_,
            &presentation::TimelineDocumentViewModel::FilterChanged, this,
            &SpreadsheetExportViewModel::availabilityChanged);
    connect(&document_view_model_,
            &presentation::TimelineDocumentViewModel::EventProjectionChanged,
            this, [this](void) {
                emit availabilityChanged();
                emit renderedVideoRequirementChanged();
            });
    connect(&document_view_model_,
            &presentation::TimelineDocumentViewModel::ExportStateChanged, this,
            [this](void) {
                emit busyChanged();
                emit availabilityChanged();
            });
    connect(&document_view_model_,
            &presentation::TimelineDocumentViewModel::
                FrameExtractionProgressChanged,
            this, [this](qulonglong, qulonglong) { emit progressChanged(); });
    connect(&document_view_model_,
            &presentation::TimelineDocumentViewModel::ExportFinished, this,
            &SpreadsheetExportViewModel::HandleExportFinished);
}

bool SpreadsheetExportViewModel::IsAvailable(void) const noexcept {
    return !exporter_identifier_.empty() && document_view_model_.CanExport();
}

bool SpreadsheetExportViewModel::IsBusy(void) const noexcept {
    return document_view_model_.ExportState() ==
           presentation::TimelineExportState::kExporting;
}

bool SpreadsheetExportViewModel::IsRenderedVideoRequired(void) const noexcept {
    return std::ranges::contains(document_view_model_.EventProjection(),
                                 core::TimelineEventField::kInitialFrame);
}

qulonglong
SpreadsheetExportViewModel::CompletedFrameCount(void) const noexcept {
    return document_view_model_.ExtractedFrameCount();
}

qulonglong SpreadsheetExportViewModel::TotalFrameCount(void) const noexcept {
    return document_view_model_.TotalFrameCount();
}

QUrl SpreadsheetExportViewModel::SuggestedDestinationUrl(void) const {
    const QFileInfo source{PathText(document_view_model_.SourcePath())};
    auto base_name = source.completeBaseName();
    if (base_name.isEmpty()) {
        base_name = tr("timeline");
    }
    const auto suggested_name = base_name + QStringLiteral("-report.xlsx");
    const QSettings settings;
    auto directory =
        settings.value(QStringLiteral("export/lastDirectory")).toString();
    if (directory.isEmpty()) {
        directory = source.absolutePath();
    }
    return QUrl::fromLocalFile(QDir{directory}.filePath(suggested_name));
}

QUrl SpreadsheetExportViewModel::SuggestedVideoFolderUrl(void) const {
    const QFileInfo source{PathText(document_view_model_.SourcePath())};
    return QUrl::fromLocalFile(source.absolutePath());
}

QString SpreadsheetExportViewModel::ResultPath(void) const {
    return result_path_;
}

QString SpreadsheetExportViewModel::ResultDetailsText(void) const {
    return result_details_text_;
}

QString SpreadsheetExportViewModel::WarningText(void) const {
    return warning_text_;
}

QString SpreadsheetExportViewModel::ErrorText(void) const {
    return error_text_;
}

bool SpreadsheetExportViewModel::HasWarnings(void) const noexcept {
    return !warning_text_.isEmpty();
}

bool SpreadsheetExportViewModel::DestinationExists(
    const QUrl &destination) const {
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty()) {
        return false;
    }
    std::error_code exists_error;
    const auto exists = std::filesystem::exists(
        FilesystemPath(destination.toLocalFile()), exists_error);
    return !exists_error && exists;
}

bool SpreadsheetExportViewModel::Start(const QUrl &destination,
                                       const QString &workbook_language,
                                       bool include_timeline_sheet,
                                       bool include_diagnostics_sheet,
                                       const QUrl &rendered_video,
                                       bool replace_existing) {
    ClearResult();
    if (exporter_identifier_.empty()) {
        SetImmediateFailure(
            tr("No registered exporter can create an Excel workbook."));
        return false;
    }
    if (!document_view_model_.CanExport()) {
        SetImmediateFailure(tr("The current timeline cannot be exported."));
        return false;
    }
    if (!destination.isLocalFile() || destination.toLocalFile().isEmpty()) {
        SetImmediateFailure(tr("Select a local destination for the workbook."));
        return false;
    }
    if (workbook_language != QStringLiteral("en") &&
        workbook_language != QStringLiteral("pt-BR")) {
        SetImmediateFailure(tr("Select a supported workbook language."));
        return false;
    }

    std::optional<std::filesystem::path> video_path;
    if (IsRenderedVideoRequired()) {
        if (!rendered_video.isLocalFile() ||
            rendered_video.toLocalFile().isEmpty()) {
            SetImmediateFailure(
                tr("Select a matching rendered video for the Initial Frame "
                   "column."));
            return false;
        }
        video_path = FilesystemPath(rendered_video.toLocalFile());
    }

    const auto destination_path = FilesystemPath(destination.toLocalFile());

    QSettings settings;
    settings.setValue(QStringLiteral("export/lastDirectory"),
                      QFileInfo{destination.toLocalFile()}.absolutePath());
    const auto result =
        document_view_model_.Export(presentation::TimelineExportRequest{
            .path = destination_path,
            .format_identifier = exporter_identifier_,
            .options =
                {
                    core::MetadataEntry{
                        .key =
                            std::string{formats::xlsx::kWorkbookLanguageOption},
                        .value = workbook_language.toStdString(),
                    },
                    core::MetadataEntry{
                        .key =
                            std::string{
                                formats::xlsx::kIncludeTimelineSheetOption},
                        .value = include_timeline_sheet,
                    },
                    core::MetadataEntry{
                        .key =
                            std::string{
                                formats::xlsx::kIncludeDiagnosticsSheetOption},
                        .value = include_diagnostics_sheet,
                    },
                },
            .video_path = std::move(video_path),
            .replace_existing = replace_existing,
        });
    if (!result.has_value()) {
        SetImmediateFailure(tr("The spreadsheet export could not be started."));
        return false;
    }
    return true;
}

void SpreadsheetExportViewModel::Cancel(void) {
    if (IsBusy()) {
        document_view_model_.CancelExport();
    }
}

bool SpreadsheetExportViewModel::RevealResult(void) const {
    return !result_path_.isEmpty() &&
           presentation::desktop_integration::RevealFile(result_path_);
}

void SpreadsheetExportViewModel::ClearResult(void) {
    if (result_path_.isEmpty() && result_details_text_.isEmpty() &&
        warning_text_.isEmpty() && error_text_.isEmpty()) {
        return;
    }
    result_path_.clear();
    result_details_text_.clear();
    warning_text_.clear();
    error_text_.clear();
    emit resultChanged();
}

void SpreadsheetExportViewModel::HandleExportFinished(void) {
    const auto *result = document_view_model_.ExportResult();
    if (result == nullptr) {
        return;
    }
    if (!result->has_value()) {
        if (HasDiagnosticCode(
                result->error().diagnostics,
                services::timeline_frame_extraction_diagnostic_code::
                    kCancelled)) {
            emit exportCancelled();
            return;
        }
        error_text_ = RenderedVideoFailureText(result->error().kind);
        const auto diagnostic_text =
            presentation::diagnostic_text::Summary(result->error().diagnostics);
        if (!diagnostic_text.isEmpty()) {
            if (!error_text_.isEmpty()) {
                error_text_ += QStringLiteral("\n\n");
            }
            error_text_ += diagnostic_text;
        }
        if (error_text_.isEmpty()) {
            error_text_ = tr("The spreadsheet could not be exported.");
        }
        emit resultChanged();
        emit exportFailed();
        return;
    }

    const auto &receipt = result->value();
    result_path_ = PathText(receipt.document_export.path);
    warning_text_ = presentation::diagnostic_text::Summary(
        receipt.document_export.diagnostics);
    if (receipt.video_information.has_value() &&
        receipt.video_mapping.has_value()) {
        const auto &stream =
            receipt.video_information
                ->streams[receipt.video_information->selected_video_stream];
        result_details_text_ =
            tr("Embedded initial-frame images from %1 (%2, %3×%4, %5/%6 "
               "fps, starting at %7); decoded %8 unique frame(s).")
                .arg(QFileInfo{PathText(receipt.video_information->path)}
                         .fileName())
                .arg(Utf8(receipt.video_information->container_long_name))
                .arg(stream.width)
                .arg(stream.height)
                .arg(receipt.video_mapping->video_start_timecode.rate()
                         .numerator())
                .arg(receipt.video_mapping->video_start_timecode.rate()
                         .denominator())
                .arg(TimecodeText(receipt.video_mapping->video_start_timecode))
                .arg(static_cast<qulonglong>(receipt.unique_frame_count));
    }
    emit resultChanged();
    emit exportSucceeded();
}

void SpreadsheetExportViewModel::SetImmediateFailure(QString error) {
    error_text_ = std::move(error);
    emit resultChanged();
    emit exportFailed();
}

} // namespace edit_atlas::frontends::quick
