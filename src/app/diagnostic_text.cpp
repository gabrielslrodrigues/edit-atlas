#include <edit_atlas/app/diagnostic_text.hpp>

#include <edit_atlas/core/document_pipeline.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <string>
#include <string_view>
#include <vector>

namespace edit_atlas::app::diagnostic_text {
namespace {

[[nodiscard]] QString Translate(const char *text) {
    return QCoreApplication::translate("edit_atlas::app::DiagnosticText", text);
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
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The input was decoded as Windows-1252."));
    }
    if (code == cmx3600::diagnostic_code::kInvalidEncoding) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The input text encoding is invalid."));
    }
    if (code == cmx3600::diagnostic_code::kMissingFrameRate) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::app::DiagnosticText",
            "A frame rate is required for this non-drop-frame EDL."));
    }
    if (code == cmx3600::diagnostic_code::kInvalidFrameRate) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::app::DiagnosticText",
            "The selected frame rate is invalid or incompatible."));
    }
    if (code == cmx3600::diagnostic_code::kMalformedEvent) {
        return Translate(QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                                           "An event record is malformed."));
    }
    if (code == cmx3600::diagnostic_code::kInvalidTimecode) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::app::DiagnosticText",
            "An event contains an invalid timecode or range."));
    }
    if (code == cmx3600::diagnostic_code::kUnknownContent) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "Unrecognized CMX 3600 content was preserved."));
    }
    if (code == cmx3600::diagnostic_code::kOrphanRecord) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::app::DiagnosticText",
            "A motion-effect record has no preceding event."));
    }

    if (code == core::pipeline_diagnostic_code::kUnknownImportFormat) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "No registered importer recognizes this file."));
    }
    if (code == core::pipeline_diagnostic_code::kAmbiguousImportFormat) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "More than one importer matches this file."));
    }
    if (code == core::pipeline_diagnostic_code::kProbeException ||
        code == core::pipeline_diagnostic_code::kImportException) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The importer failed unexpectedly."));
    }
    if (code == core::pipeline_diagnostic_code::kImportProducedNoDocument) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The importer did not produce a timeline."));
    }
    if (code == core::pipeline_diagnostic_code::kUnknownExportFormat) {
        return Translate(QT_TRANSLATE_NOOP(
            "edit_atlas::app::DiagnosticText",
            "The requested export format is not registered."));
    }
    if (code == core::pipeline_diagnostic_code::kExportException) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The exporter failed unexpectedly."));
    }
    if (code == core::pipeline_diagnostic_code::kExportProducedNoArtifact) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The exporter did not produce a spreadsheet."));
    }
    if (code == formats::xlsx::diagnostic_code::kWorkbookCreationFailed) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The Excel workbook could not be created."));
    }
    if (code == formats::xlsx::diagnostic_code::kWorkbookWriteFailed) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The Excel workbook could not be written."));
    }
    if (code == formats::xlsx::diagnostic_code::kInvalidEventProjection) {
        return Translate(
            QT_TRANSLATE_NOOP("edit_atlas::app::DiagnosticText",
                              "The event column selection is invalid."));
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

} // namespace edit_atlas::app::diagnostic_text
