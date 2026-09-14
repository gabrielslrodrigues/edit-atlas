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

#ifndef EDIT_ATLAS_FRONTENDS_CLI_DIAGNOSTIC_OUTPUT_HPP_
#define EDIT_ATLAS_FRONTENDS_CLI_DIAGNOSTIC_OUTPUT_HPP_

#include <ostream>
#include <string_view>
#include <vector>

#include "edit_atlas/core/editorial_timeline.hpp"

namespace edit_atlas::frontends::cli {

void WriteDiagnostics(const std::vector<core::Diagnostic>& diagnostics,
                      std::ostream& output);

void WriteFailure(std::string_view message,
                  const std::vector<core::Diagnostic>& diagnostics,
                  std::ostream& error);

}  // namespace edit_atlas::frontends::cli

#endif  // EDIT_ATLAS_FRONTENDS_CLI_DIAGNOSTIC_OUTPUT_HPP_
