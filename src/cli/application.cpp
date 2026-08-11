#include <edit_atlas/cli/application.hpp>

#include "diagnostic_output.hpp"

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>
#include <edit_atlas/core/version.hpp>
#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>
#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::cli {
namespace {

struct Options final {
    std::string input;
    std::string output;
    std::optional<std::string> frame_rate;
    std::string workbook_language = "pt-BR";
    std::optional<std::string> columns;
    bool omit_timeline_sheet = false;
    bool omit_diagnostics_sheet = false;
    bool replace_existing = false;
};

[[nodiscard]] CLI::App *ConfigureApplication(CLI::App &application,
                                             Options &options) {
    application.set_version_flag("--version", std::string{"Edit Atlas "} +
                                                  std::string{core::Version()});
    application.require_subcommand(1);

    auto *convert = application.add_subcommand(
        "convert", "Convert a local CMX 3600 EDL into an XLSX report");
    convert->add_option("input", options.input, "Input CMX 3600 EDL path")
        ->required();
    convert->add_option("output", options.output, "Output XLSX path")
        ->required();
    convert
        ->add_option("--fps", options.frame_rate,
                     "Frame rate for non-drop-frame EDLs, such as 24, 25, or "
                     "30000/1001")
        ->multi_option_policy(CLI::MultiOptionPolicy::Throw);
    convert
        ->add_option("--language", options.workbook_language,
                     "Workbook language")
        ->check(CLI::IsMember({"en", "pt-BR"}))
        ->capture_default_str()
        ->multi_option_policy(CLI::MultiOptionPolicy::Throw);
    convert
        ->add_option("--columns", options.columns,
                     "Ordered event columns, separated by commas")
        ->multi_option_policy(CLI::MultiOptionPolicy::Throw);
    convert->add_flag("--no-timeline-sheet", options.omit_timeline_sheet,
                      "Omit the timeline summary sheet");
    convert->add_flag("--no-diagnostics-sheet", options.omit_diagnostics_sheet,
                      "Omit the diagnostics sheet");
    convert->add_flag("--force", options.replace_existing,
                      "Replace an existing output file");
    return convert;
}

void ParseArguments(CLI::App &application,
                    std::span<const std::string_view> arguments) {
    std::vector<std::string> owned_arguments;
    owned_arguments.reserve(arguments.size() + 1);
    owned_arguments.emplace_back("edit-atlas-cli");
    for (const auto argument : arguments) {
        owned_arguments.emplace_back(argument);
    }

    std::vector<const char *> argument_pointers;
    argument_pointers.reserve(owned_arguments.size());
    for (const auto &argument : owned_arguments) {
        argument_pointers.emplace_back(argument.c_str());
    }
    application.parse(static_cast<int>(argument_pointers.size()),
                      argument_pointers.data());
}

[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value) {
    const auto *begin = reinterpret_cast<const char8_t *>(value.data());
    return std::filesystem::path{std::u8string{begin, begin + value.size()}};
}

[[nodiscard]] std::string PathToUtf8(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return std::string{reinterpret_cast<const char *>(utf8.data()),
                       utf8.size()};
}

[[nodiscard]] bool HasSeverity(const std::vector<core::Diagnostic> &diagnostics,
                               core::DiagnosticSeverity severity) {
    return std::ranges::any_of(diagnostics,
                               [severity](const core::Diagnostic &diagnostic) {
                                   return diagnostic.severity == severity;
                               });
}

[[nodiscard]] core::Diagnostic
FilesystemDiagnostic(std::string code, std::string message,
                     const std::filesystem::path &path) {
    return core::Diagnostic{
        .severity = core::DiagnosticSeverity::kError,
        .code = std::move(code),
        .message = std::move(message),
        .location =
            core::SourceLocation{
                .source = PathToUtf8(path),
                .line = 0,
                .column = 0,
            },
    };
}

[[nodiscard]] std::string
ImportFailureMessage(const services::TimelineDocumentImportFailure &failure) {
    switch (failure.kind) {
    case services::TimelineDocumentImportFailureKind::kOpenFailed:
        return "Could not open the input file: " +
               failure.filesystem_error.message();
    case services::TimelineDocumentImportFailureKind::kReadFailed:
        return "Could not read the input file: " +
               failure.filesystem_error.message();
    case services::TimelineDocumentImportFailureKind::kImportFailed:
        return "The input could not be imported";
    }
    return "The input could not be imported";
}

[[nodiscard]] std::string
ImportFailureCode(services::TimelineDocumentImportFailureKind kind) {
    return kind == services::TimelineDocumentImportFailureKind::kOpenFailed
               ? "cli.input.open_failed"
               : "cli.input.read_failed";
}

[[nodiscard]] std::string
ExportFailureMessage(const services::TimelineDocumentExportFailure &failure) {
    switch (failure.kind) {
    case services::TimelineDocumentExportFailureKind::kInvalidRequest:
        return "The XLSX report request is invalid";
    case services::TimelineDocumentExportFailureKind::kExportFailed:
        return "The XLSX report could not be created";
    case services::TimelineDocumentExportFailureKind::kDestinationExists:
        return "The output file already exists; use --force to replace it";
    case services::TimelineDocumentExportFailureKind::kWriteFailed:
        return "Could not write the output file: " +
               failure.filesystem_error.message();
    case services::TimelineDocumentExportFailureKind::kCommitFailed:
        return "Could not commit the output file: " +
               failure.filesystem_error.message();
    }
    return "The XLSX report could not be created";
}

[[nodiscard]] std::string
ExportFailureCode(services::TimelineDocumentExportFailureKind kind) {
    switch (kind) {
    case services::TimelineDocumentExportFailureKind::kInvalidRequest:
        return "cli.output.invalid_request";
    case services::TimelineDocumentExportFailureKind::kDestinationExists:
        return "cli.output.destination_exists";
    case services::TimelineDocumentExportFailureKind::kWriteFailed:
        return "cli.output.write_failed";
    case services::TimelineDocumentExportFailureKind::kCommitFailed:
        return "cli.output.commit_failed";
    case services::TimelineDocumentExportFailureKind::kExportFailed:
        return "cli.output.export_failed";
    }
    return "cli.output.export_failed";
}

[[nodiscard]] std::vector<core::MetadataEntry>
ImportOptions(const Options &options) {
    if (!options.frame_rate.has_value()) {
        return {};
    }
    return {
        core::MetadataEntry{
            .key = std::string{formats::cmx3600::kFrameRateOption},
            .value = *options.frame_rate,
        },
    };
}

[[nodiscard]] std::vector<core::MetadataEntry>
ExportOptions(const Options &options) {
    return {
        core::MetadataEntry{
            .key = std::string{formats::xlsx::kWorkbookLanguageOption},
            .value = options.workbook_language,
        },
        core::MetadataEntry{
            .key = std::string{formats::xlsx::kIncludeTimelineSheetOption},
            .value = !options.omit_timeline_sheet,
        },
        core::MetadataEntry{
            .key = std::string{formats::xlsx::kIncludeDiagnosticsSheetOption},
            .value = !options.omit_diagnostics_sheet,
        },
    };
}

[[nodiscard]] std::optional<std::vector<core::TimelineEventField>>
EventProjection(const Options &options) {
    if (!options.columns.has_value()) {
        return std::vector<core::TimelineEventField>{
            core::DefaultTimelineEventProjection().begin(),
            core::DefaultTimelineEventProjection().end(),
        };
    }
    std::vector<core::TimelineEventField> projection;
    auto identifiers = std::string_view{*options.columns};
    while (!identifiers.empty()) {
        const auto separator = identifiers.find(',');
        const auto identifier = identifiers.substr(0, separator);
        const auto field = core::TimelineEventFieldFromIdentifier(identifier);
        if (!field.has_value()) {
            return std::nullopt;
        }
        projection.push_back(*field);
        if (separator == std::string_view::npos) {
            break;
        }
        if (separator + 1 == identifiers.size()) {
            return std::nullopt;
        }
        identifiers.remove_prefix(separator + 1);
    }
    if (!core::IsValidTimelineEventProjection(projection)) {
        return std::nullopt;
    }
    return projection;
}

[[nodiscard]] ExitCode Convert(const Options &options, std::ostream &output,
                               std::ostream &error) {
    auto event_projection = EventProjection(options);
    if (!event_projection.has_value()) {
        error << "--columns must contain comma-separated unique column "
                 "identifiers\n";
        return ExitCode::kUsageError;
    }
    auto registry = services::CreateBuiltInFormatRegistry();
    if (!registry.has_value()) {
        constexpr std::string_view kMessage =
            "Could not initialize the built-in format registry";
        WriteFailure(kMessage,
                     {
                         core::Diagnostic{
                             .severity = core::DiagnosticSeverity::kError,
                             .code = "cli.registry.initialization_failed",
                             .message = std::string{kMessage},
                             .location = std::nullopt,
                         },
                     },
                     error);
        return ExitCode::kOperationalFailure;
    }

    const services::TimelineDocumentImportService import_service{*registry};
    auto import_result =
        import_service.Import(services::TimelineDocumentImportRequest{
            .path = PathFromUtf8(options.input),
            .format_identifier = std::nullopt,
            .options = ImportOptions(options),
        });
    if (!import_result.has_value()) {
        auto failure = std::move(import_result.error());
        const auto message = ImportFailureMessage(failure);
        if (failure.kind !=
            services::TimelineDocumentImportFailureKind::kImportFailed) {
            failure.diagnostics.emplace_back(FilesystemDiagnostic(
                ImportFailureCode(failure.kind), message, failure.path));
        }
        WriteFailure(message, failure.diagnostics, error);
        return failure.kind == services::TimelineDocumentImportFailureKind::
                                   kImportFailed
                   ? ExitCode::kInvalidInput
                   : ExitCode::kOperationalFailure;
    }

    auto diagnostics = std::move(import_result->diagnostics);
    if (HasSeverity(diagnostics, core::DiagnosticSeverity::kError)) {
        WriteFailure("The input contains errors", diagnostics, error);
        return ExitCode::kInvalidInput;
    }

    const auto event_count = import_result->timeline.events.size();
    const services::TimelineDocumentExportService export_service{*registry};
    auto export_result =
        export_service.Export(services::TimelineDocumentExportRequest{
            .path = PathFromUtf8(options.output),
            .format_identifier = std::string{formats::xlsx::kFormatIdentifier},
            .timeline = std::move(import_result->timeline),
            .event_projection = std::move(*event_projection),
            .options = ExportOptions(options),
            .event_images = {},
            .replace_existing = options.replace_existing,
        });
    if (!export_result.has_value()) {
        auto failure = std::move(export_result.error());
        const auto message = ExportFailureMessage(failure);
        if (failure.diagnostics.empty()) {
            diagnostics.emplace_back(FilesystemDiagnostic(
                ExportFailureCode(failure.kind), message, failure.path));
        } else {
            diagnostics.insert(diagnostics.end(), failure.diagnostics.begin(),
                               failure.diagnostics.end());
        }
        WriteFailure(message, diagnostics, error);
        return failure.kind == services::TimelineDocumentExportFailureKind::
                                   kDestinationExists
                   ? ExitCode::kDestinationExists
                   : ExitCode::kOperationalFailure;
    }

    diagnostics.insert(diagnostics.end(), export_result->diagnostics.begin(),
                       export_result->diagnostics.end());
    output << "Converted " << event_count << " event(s) from "
           << PathToUtf8(import_result->path) << " to "
           << PathToUtf8(export_result->path) << '\n';
    WriteDiagnostics(diagnostics, error);
    if (HasSeverity(diagnostics, core::DiagnosticSeverity::kError)) {
        return ExitCode::kOperationalFailure;
    }
    return HasSeverity(diagnostics, core::DiagnosticSeverity::kWarning)
               ? ExitCode::kWarnings
               : ExitCode::kSuccess;
}

} // namespace

ExitCode Run(std::span<const std::string_view> arguments, std::ostream &output,
             std::ostream &error) {
    try {
        Options options;
        CLI::App application{
            "Inspect editorial timelines and export structured reports",
            "edit-atlas-cli"};
        const auto *convert = ConfigureApplication(application, options);
        try {
            ParseArguments(application, arguments);
        } catch (const CLI::ParseError &parse_error) {
            const auto cli11_exit_code =
                application.exit(parse_error, output, error);
            return cli11_exit_code == 0 ? ExitCode::kSuccess
                                        : ExitCode::kUsageError;
        }
        if (convert->parsed()) {
            return Convert(options, output, error);
        }
        return ExitCode::kUsageError;
    } catch (const std::exception &exception) {
        WriteFailure(exception.what(),
                     {
                         core::Diagnostic{
                             .severity = core::DiagnosticSeverity::kError,
                             .code = "cli.operation.exception",
                             .message = exception.what(),
                             .location = std::nullopt,
                         },
                     },
                     error);
        return ExitCode::kOperationalFailure;
    } catch (...) {
        constexpr std::string_view kMessage =
            "The command failed with an unknown error";
        WriteFailure(kMessage,
                     {
                         core::Diagnostic{
                             .severity = core::DiagnosticSeverity::kError,
                             .code = "cli.operation.exception",
                             .message = std::string{kMessage},
                             .location = std::nullopt,
                         },
                     },
                     error);
        return ExitCode::kOperationalFailure;
    }
}

} // namespace edit_atlas::cli
