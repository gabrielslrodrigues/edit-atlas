// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "diagnostic_output.hpp"

#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

#include "edit_atlas/core/editorial_timeline.hpp"

namespace edit_atlas::frontends::cli {
namespace {

[[nodiscard]] std::string_view SeverityName(
    core::DiagnosticSeverity severity) noexcept {
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

void WriteLocation(const std::optional<core::SourceLocation>& location,
                   std::ostream& output) {
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

}  // namespace

void WriteDiagnostics(const std::vector<core::Diagnostic>& diagnostics,
                      std::ostream& output) {
  for (const auto& diagnostic : diagnostics) {
    output << SeverityName(diagnostic.severity) << " [" << diagnostic.code
           << "] ";
    WriteLocation(diagnostic.location, output);
    output << diagnostic.message << '\n';
  }
}

void WriteFailure(std::string_view message,
                  const std::vector<core::Diagnostic>& diagnostics,
                  std::ostream& error) {
  error << "error: " << message << '\n';
  WriteDiagnostics(diagnostics, error);
}

}  // namespace edit_atlas::frontends::cli
