#include "diagnostic_output.hpp"

#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

namespace edit_atlas::cli {
namespace {

[[nodiscard]] std::string_view
SeverityName(core::DiagnosticSeverity severity) noexcept {
    switch (severity) {
    case core::DiagnosticSeverity::kInfo:
        return "info";
    case core::DiagnosticSeverity::kWarning:
        return "warning";
    case core::DiagnosticSeverity::kError:
        return "error";
    }
    return "error";
}

void WriteLocation(const std::optional<core::SourceLocation> &location,
                   std::ostream &output) {
    if (!location.has_value()) {
        return;
    }
    output << location->source;
    if (location->line != 0) {
        output << ':' << location->line;
        if (location->column != 0) {
            output << ':' << location->column;
        }
    }
    output << ": ";
}

} // namespace

void WriteDiagnostics(const std::vector<core::Diagnostic> &diagnostics,
                      std::ostream &output) {
    for (const auto &diagnostic : diagnostics) {
        output << SeverityName(diagnostic.severity) << " [" << diagnostic.code
               << "] ";
        WriteLocation(diagnostic.location, output);
        output << diagnostic.message << '\n';
    }
}

void WriteFailure(std::string_view message,
                  const std::vector<core::Diagnostic> &diagnostics,
                  std::ostream &error) {
    error << "error: " << message << '\n';
    WriteDiagnostics(diagnostics, error);
}

} // namespace edit_atlas::cli
