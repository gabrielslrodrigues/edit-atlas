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

#include "gtest/gtest.h"

#include "edit_atlas/formats/cmx3600/cmx3600_importer.hpp"
#include "edit_atlas/formats/xlsx/xlsx_exporter.hpp"

namespace edit_atlas::services {
namespace {

TEST(BuiltInFormatsTest, RegistersEveryBuiltInFormatHandler) {
  const auto registry = CreateBuiltInFormatRegistry();

  ASSERT_TRUE(registry.has_value());
  EXPECT_NE(registry->FindImporter(formats::cmx3600::kFormatIdentifier),
            nullptr);
  EXPECT_NE(registry->FindExporter(formats::xlsx::kFormatIdentifier), nullptr);
  EXPECT_EQ(registry->importer_formats().size(), 1);
  EXPECT_EQ(registry->exporter_formats().size(), 1);
}

}  // namespace
}  // namespace edit_atlas::services
