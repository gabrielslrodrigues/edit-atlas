#ifndef EDIT_ATLAS_FRONTENDS_CLI_DIAGNOSTIC_OUTPUT_HPP_
#define EDIT_ATLAS_FRONTENDS_CLI_DIAGNOSTIC_OUTPUT_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <iosfwd>
#include <string_view>
#include <vector>

namespace edit_atlas::frontends::cli {

void WriteDiagnostics(const std::vector<core::Diagnostic> &diagnostics,
                      std::ostream &output);

void WriteFailure(std::string_view message,
                  const std::vector<core::Diagnostic> &diagnostics,
                  std::ostream &error);

} // namespace edit_atlas::frontends::cli

#endif // EDIT_ATLAS_FRONTENDS_CLI_DIAGNOSTIC_OUTPUT_HPP_
