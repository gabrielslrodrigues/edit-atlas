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

#ifndef EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_TEXT_HPP_
#define EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_TEXT_HPP_

#include <vector>

#include "QString"

#include "edit_atlas/core/editorial_timeline.hpp"

namespace edit_atlas::presentation::diagnostic_text {

/// Returns a localized presentation message for a diagnostic.
[[nodiscard]] QString Message(const core::Diagnostic& diagnostic);

/// Returns a localized, human-readable diagnostic list.
[[nodiscard]] QString Summary(const std::vector<core::Diagnostic>& diagnostics);

}  // namespace edit_atlas::presentation::diagnostic_text

#endif  // EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_TEXT_HPP_
