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

#include "edit_atlas/services/built_in_formats.hpp"

#include <expected>
#include <memory>

#include "edit_atlas/core/format_registry.hpp"
#include "edit_atlas/formats/cmx3600/cmx3600_importer.hpp"
#include "edit_atlas/formats/xlsx/xlsx_exporter.hpp"

namespace edit_atlas::services {

std::expected<core::FormatRegistry, core::FormatRegistrationError>
CreateBuiltInFormatRegistry(void) {
  core::FormatRegistry registry;
  if (const auto result = registry.RegisterImporter(
          std::make_unique<formats::cmx3600::Cmx3600Importer>());
      !result.has_value()) {
    return std::unexpected(result.error());
  }
  if (const auto result = registry.RegisterExporter(
          std::make_unique<formats::xlsx::XlsxExporter>());
      !result.has_value()) {
    return std::unexpected(result.error());
  }
  return registry;
}

}  // namespace edit_atlas::services
