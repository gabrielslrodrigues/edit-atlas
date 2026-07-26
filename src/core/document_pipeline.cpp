#include <edit_atlas/core/document_pipeline.hpp>

#include <algorithm>
#include <exception>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace edit_atlas::core {
namespace {

[[nodiscard]] Diagnostic MakeDiagnostic(DiagnosticSeverity severity,
                                        std::string_view code,
                                        std::string message) {
    return Diagnostic{
        .severity = severity,
        .code = std::string{code},
        .message = std::move(message),
        .location = std::nullopt,
    };
}

[[nodiscard]] bool HasError(const std::vector<Diagnostic> &diagnostics) {
    return std::ranges::any_of(diagnostics, [](const Diagnostic &diagnostic) {
        return diagnostic.severity == DiagnosticSeverity::kError;
    });
}

[[nodiscard]] std::string_view
ExtensionFromRequest(const ImportRequest &request) noexcept {
    if (!request.extension.empty()) {
        return request.extension;
    }

    const auto separator = request.source_name.find_last_of("/\\");
    const auto dot = request.source_name.find_last_of('.');
    if (dot == std::string::npos ||
        (separator != std::string::npos && dot < separator) ||
        dot + 1 >= request.source_name.size()) {
        return {};
    }
    return std::string_view{request.source_name}.substr(dot + 1);
}

struct ProbeResult final {
    const Importer *importer;
    ProbeConfidence confidence;
};

[[nodiscard]] const Importer *
BreakProbeTieWithExtension(const std::vector<ProbeResult> &matches,
                           const std::vector<const Importer *> &by_extension) {
    const Importer *selected = nullptr;
    for (const auto &match : matches) {
        if (std::ranges::find(by_extension, match.importer) ==
            by_extension.end()) {
            continue;
        }
        if (selected != nullptr) {
            return nullptr;
        }
        selected = match.importer;
    }
    return selected;
}

[[nodiscard]] const Importer *
SelectImporter(const FormatRegistry &registry, const ImportRequest &request,
               std::vector<Diagnostic> &diagnostics, bool &ambiguous) {
    const auto extension = ExtensionFromRequest(request);
    const auto by_extension = registry.ImportersForExtension(extension);
    std::vector<ProbeResult> probes;

    for (const auto *importer : registry.importers()) {
        try {
            const auto confidence = importer->Probe(request);
            if (confidence != ProbeConfidence::kNone) {
                probes.emplace_back(importer, confidence);
            }
        } catch (const std::exception &exception) {
            diagnostics.emplace_back(MakeDiagnostic(
                DiagnosticSeverity::kWarning,
                pipeline_diagnostic_code::kProbeException,
                "Importer '" + importer->descriptor().identifier +
                    "' failed while probing: " + exception.what()));
        } catch (...) {
            diagnostics.emplace_back(MakeDiagnostic(
                DiagnosticSeverity::kWarning,
                pipeline_diagnostic_code::kProbeException,
                "Importer '" + importer->descriptor().identifier +
                    "' failed while probing"));
        }
    }

    if (probes.empty()) {
        ambiguous = by_extension.size() > 1;
        return by_extension.size() == 1 ? by_extension.front() : nullptr;
    }

    const auto strongest =
        std::ranges::max_element(probes, {}, &ProbeResult::confidence)
            ->confidence;
    std::vector<ProbeResult> strongest_matches;
    std::ranges::copy_if(probes, std::back_inserter(strongest_matches),
                         [strongest](const ProbeResult &result) {
                             return result.confidence == strongest;
                         });

    if (strongest_matches.size() == 1) {
        return strongest_matches.front().importer;
    }
    const auto *selected =
        BreakProbeTieWithExtension(strongest_matches, by_extension);
    ambiguous = selected == nullptr;
    return selected;
}

[[nodiscard]] ImportResult FailedImport(std::vector<Diagnostic> diagnostics,
                                        std::string_view code,
                                        std::string message) {
    diagnostics.emplace_back(
        MakeDiagnostic(DiagnosticSeverity::kError, code, std::move(message)));
    return ImportResult{
        .document = std::nullopt,
        .diagnostics = std::move(diagnostics),
    };
}

[[nodiscard]] ExportResult FailedExport(std::string_view code,
                                        std::string message) {
    return ExportResult{
        .artifact = std::nullopt,
        .diagnostics =
            {
                MakeDiagnostic(DiagnosticSeverity::kError, code,
                               std::move(message)),
            },
    };
}

} // namespace

DocumentPipeline::DocumentPipeline(const FormatRegistry &registry) noexcept
    : registry_(registry) {}

ImportResult DocumentPipeline::Import(
    const ImportRequest &request,
    std::optional<std::string_view> format_identifier) const {
    std::vector<Diagnostic> discovery_diagnostics;
    const Importer *importer = nullptr;

    if (format_identifier.has_value()) {
        importer = registry_.FindImporter(*format_identifier);
        if (importer == nullptr) {
            return FailedImport({},
                                pipeline_diagnostic_code::kUnknownImportFormat,
                                "No importer is registered for format '" +
                                    std::string{*format_identifier} + "'");
        }
    } else {
        bool ambiguous = false;
        importer = SelectImporter(registry_, request, discovery_diagnostics,
                                  ambiguous);
        if (importer == nullptr) {
            const auto code =
                ambiguous ? pipeline_diagnostic_code::kAmbiguousImportFormat
                          : pipeline_diagnostic_code::kUnknownImportFormat;
            const auto message =
                code == pipeline_diagnostic_code::kAmbiguousImportFormat
                    ? "Multiple importers match the supplied content"
                    : "No registered importer recognizes the supplied content";
            return FailedImport(std::move(discovery_diagnostics), code,
                                message);
        }
    }

    try {
        auto result = importer->Import(request);
        discovery_diagnostics.insert(
            discovery_diagnostics.end(),
            std::make_move_iterator(result.diagnostics.begin()),
            std::make_move_iterator(result.diagnostics.end()));
        result.diagnostics = std::move(discovery_diagnostics);
        if (!result.document.has_value() && !HasError(result.diagnostics)) {
            result.diagnostics.emplace_back(MakeDiagnostic(
                DiagnosticSeverity::kError,
                pipeline_diagnostic_code::kImportProducedNoDocument,
                "Importer '" + importer->descriptor().identifier +
                    "' produced no document"));
        }
        return result;
    } catch (const std::exception &exception) {
        return FailedImport(std::move(discovery_diagnostics),
                            pipeline_diagnostic_code::kImportException,
                            "Importer '" + importer->descriptor().identifier +
                                "' failed: " + exception.what());
    } catch (...) {
        return FailedImport(std::move(discovery_diagnostics),
                            pipeline_diagnostic_code::kImportException,
                            "Importer '" + importer->descriptor().identifier +
                                "' failed");
    }
}

ExportResult
DocumentPipeline::Export(const ExportRequest &request,
                         std::string_view format_identifier) const {
    const auto *exporter = registry_.FindExporter(format_identifier);
    if (exporter == nullptr) {
        return FailedExport(pipeline_diagnostic_code::kUnknownExportFormat,
                            "No exporter is registered for format '" +
                                std::string{format_identifier} + "'");
    }

    try {
        auto result = exporter->Export(request);
        if (!result.artifact.has_value() && !HasError(result.diagnostics)) {
            result.diagnostics.emplace_back(MakeDiagnostic(
                DiagnosticSeverity::kError,
                pipeline_diagnostic_code::kExportProducedNoArtifact,
                "Exporter '" + exporter->descriptor().identifier +
                    "' produced no artifact"));
        }
        return result;
    } catch (const std::exception &exception) {
        return FailedExport(pipeline_diagnostic_code::kExportException,
                            "Exporter '" + exporter->descriptor().identifier +
                                "' failed: " + exception.what());
    } catch (...) {
        return FailedExport(pipeline_diagnostic_code::kExportException,
                            "Exporter '" + exporter->descriptor().identifier +
                                "' failed");
    }
}

} // namespace edit_atlas::core
