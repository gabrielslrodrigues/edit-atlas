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

#ifndef EDIT_ATLAS_SERVICES_BUILT_IN_FORMATS_HPP_
#define EDIT_ATLAS_SERVICES_BUILT_IN_FORMATS_HPP_

#include <expected>

#include "edit_atlas/core/format_registry.hpp"

namespace edit_atlas::services {

/// Creates a registry containing every format built into Edit Atlas.
///
/// \returns The populated registry or the first registration error.
[[nodiscard]] std::expected<core::FormatRegistry, core::FormatRegistrationError>
CreateBuiltInFormatRegistry(void);

}  // namespace edit_atlas::services

#endif  // EDIT_ATLAS_SERVICES_BUILT_IN_FORMATS_HPP_
