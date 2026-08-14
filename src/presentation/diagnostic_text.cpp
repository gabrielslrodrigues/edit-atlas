#include <edit_atlas/presentation/diagnostic_text.hpp>

#include <edit_atlas/core/timeline_document_pipeline.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/services/timeline_frame_extraction_service.hpp>
#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>
#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <string>
#include <string_view>
#include <vector>

namespace edit_atlas::presentation::diagnostic_text {
namespace {

[[nodiscard]] QString Translate(const char *text) {
    return QCoreApplication::translate(
        "edit_atlas::presentation::DiagnosticText", text);
}

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

QString Message(const core::Diagnostic &diagnostic) {
    const auto code = std::string_view{diagnostic.code};
    namespace cmx3600 = formats::cmx3600;
    if (code == cmx3600::diagnostic_code::kEncodingFallback) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The input was decoded as Windows-1252."));
    }
    if (code == cmx3600::diagnostic_code::kInvalidEncoding) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The input text encoding is invalid."));
    }
    if (code == cmx3600::diagnostic_code::kMissingFrameRate) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "A frame rate is required for this non-drop-frame EDL."));
    }
    if (code == cmx3600::diagnostic_code::kInvalidFrameRate) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The selected frame rate is invalid or incompatible."));
    }
    if (code == cmx3600::diagnostic_code::kMalformedEvent) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "An event record is malformed."));
    }
    if (code == cmx3600::diagnostic_code::kInvalidTimecode) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "An event contains an invalid timecode or range."));
    }
    if (code == cmx3600::diagnostic_code::kUnknownContent) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "Unrecognized CMX 3600 content was preserved."));
    }
    if (code == cmx3600::diagnostic_code::kOrphanRecord) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "A motion-effect record has no preceding event."));
    }

    if (code == core::pipeline_diagnostic_code::kUnknownImportFormat) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "No registered importer recognizes this file."));
    }
    if (code == core::pipeline_diagnostic_code::kAmbiguousImportFormat) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "More than one importer matches this file."));
    }
    if (code == core::pipeline_diagnostic_code::kProbeException ||
        code == core::pipeline_diagnostic_code::kImportException) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The importer failed unexpectedly."));
    }
    if (code == core::pipeline_diagnostic_code::kImportProducedNoDocument) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The importer did not produce a timeline."));
    }
    if (code == core::pipeline_diagnostic_code::kUnknownExportFormat) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The requested export format is not registered."));
    }
    if (code == core::pipeline_diagnostic_code::kExportException) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The exporter failed unexpectedly."));
    }
    if (code == core::pipeline_diagnostic_code::kExportProducedNoArtifact) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The exporter did not produce a spreadsheet."));
    }
    if (code == formats::xlsx::diagnostic_code::kWorkbookCreationFailed) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The Excel workbook could not be created."));
    }
    if (code == formats::xlsx::diagnostic_code::kWorkbookWriteFailed) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The Excel workbook could not be written."));
    }
    if (code == formats::xlsx::diagnostic_code::kInvalidEventProjection) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The event column selection is invalid."));
    }
    if (code == formats::xlsx::diagnostic_code::kImageWriteFailed) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "An initial-frame image could not be written."));
    }
    namespace video = services::timeline_video_diagnostic_code;
    if (code == services::timeline_rendered_video_export_diagnostic_code::
                    kVideoRequired) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "Select a rendered video for the Initial Frame column."));
    }
    if (code == video::kOpenFailed) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "The rendered video could not be opened."));
    }
    if (code == video::kUnsupportedContainer) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The rendered video is not a supported MOV, MP4, or MXF file."));
    }
    if (code == video::kEmptyTimeline || code == video::kInconsistentTimeline) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The imported timeline cannot be mapped to rendered-video "
            "frames."));
    }
    if (code == video::kMissingFrameRate || code == video::kVariableFrameRate) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The rendered video does not have a verifiable constant frame "
            "rate."));
    }
    if (code == video::kFrameRateMismatch) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The rendered-video frame rate does not match the timeline."));
    }
    if (code == video::kMissingTimecode) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The rendered video has no readable embedded starting timecode."));
    }
    if (code == video::kInvalidTimecode) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The rendered video's embedded timecode is invalid or "
            "ambiguous."));
    }
    if (code == video::kTimecodeModeMismatch) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The rendered-video and timeline timecode modes do not match."));
    }
    if (code == video::kTimelineStartMismatch) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The rendered-video starting timecode does not match the first "
            "record frame."));
    }
    if (code == video::kMissingDuration || code == video::kDurationMismatch) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "The rendered-video duration cannot be matched to the timeline."));
    }
    namespace extraction = services::timeline_frame_extraction_diagnostic_code;
    if (code == extraction::kCancelled) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::presentation::DiagnosticText",
                              "Initial-frame extraction was cancelled."));
    }
    if (code == extraction::kSeekFailed || code == extraction::kDecodeFailed ||
        code == extraction::kFrameUnavailable ||
        code == extraction::kInvalidFrame ||
        code == extraction::kInvalidMapping ||
        code == extraction::kInvalidOutputSize ||
        code == extraction::kProgressCallbackFailed) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::presentation::DiagnosticText",
            "An initial frame could not be extracted from the rendered "
            "video."));
    }
    return Utf8(diagnostic.message);
}

QString Summary(const std::vector<core::Diagnostic> &diagnostics) {
    QStringList messages;
    messages.reserve(static_cast<qsizetype>(diagnostics.size()));
    for (const auto &diagnostic : diagnostics) {
        messages.emplace_back(
            QStringLiteral("• %1 [%2]")
                .arg(Message(diagnostic), Utf8(diagnostic.code)));
    }
    return messages.join(u'\n');
}

} // namespace edit_atlas::presentation::diagnostic_text
