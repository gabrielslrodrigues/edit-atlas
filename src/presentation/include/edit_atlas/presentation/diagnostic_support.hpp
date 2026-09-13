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

#ifndef EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_SUPPORT_HPP_
#define EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_SUPPORT_HPP_

#include <filesystem>

#include "edit_atlas/core/format_registry.hpp"
#include "edit_atlas/support/support_bundle.hpp"

namespace edit_atlas::presentation {

/// Returns the platform-appropriate private application log directory.
[[nodiscard]] std::filesystem::path ConfiguredLogDirectory(void);

/// Collects the fixed, non-sensitive runtime metadata used for diagnostics.
[[nodiscard]] support::DiagnosticEnvironment CreateDiagnosticEnvironment(
    const core::FormatRegistry& registry);

/// Writes the diagnostic environment to the configured application logger.
void LogDiagnosticEnvironment(
    const support::DiagnosticEnvironment& environment);

}  // namespace edit_atlas::presentation

#endif  // EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_SUPPORT_HPP_
