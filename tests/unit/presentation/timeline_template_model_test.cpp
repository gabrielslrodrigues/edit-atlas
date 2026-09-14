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

#include "edit_atlas/presentation/timeline_template_model.hpp"

#include <array>
#include <optional>
#include <string_view>

#include "QString"
#include "gtest/gtest.h"

#include "edit_atlas/services/timeline_template.hpp"

namespace edit_atlas::presentation {
namespace {

TEST(TimelineTemplateModelTest, PresentsNoTemplateAndStableSavedChoices) {
  const std::array templates{
      services::TimelineTemplate{
          .identifier = "first-id",
          .name = "First",
          .filter = {},
          .event_projection = {},
      },
      services::TimelineTemplate{
          .identifier = "second-id",
          .name = "Second",
          .filter = {},
          .event_projection = {},
      },
  };
  TimelineTemplateModel model;

  model.SetTemplates(templates, std::string_view{"second-id"}, true);

  ASSERT_EQ(model.rowCount(), 3);
  EXPECT_EQ(model.data(model.index(0, 0)).toString(),
            QStringLiteral("No template"));
  EXPECT_TRUE(
      model.data(model.index(0, 0), TimelineTemplateModel::kIdentifierRole)
          .toString()
          .isEmpty());
  EXPECT_EQ(model.data(model.index(1, 0)).toString(), QStringLiteral("First"));
  EXPECT_EQ(
      model.data(model.index(2, 0), TimelineTemplateModel::kIdentifierRole)
          .toString(),
      QStringLiteral("second-id"));
  EXPECT_TRUE(model.data(model.index(2, 0), TimelineTemplateModel::kActiveRole)
                  .toBool());
  EXPECT_TRUE(
      model.data(model.index(2, 0), TimelineTemplateModel::kModifiedRole)
          .toBool());
  EXPECT_EQ(model.ActiveRow(), 2);
}

TEST(TimelineTemplateModelTest, FallsBackToTheNoTemplateChoice) {
  TimelineTemplateModel model;

  model.SetTemplates({}, std::nullopt, true);

  ASSERT_EQ(model.rowCount(), 1);
  EXPECT_EQ(model.ActiveRow(), 0);
  EXPECT_TRUE(model.data(model.index(0, 0), TimelineTemplateModel::kActiveRole)
                  .toBool());
  EXPECT_FALSE(
      model.data(model.index(0, 0), TimelineTemplateModel::kModifiedRole)
          .toBool());
}

}  // namespace
}  // namespace edit_atlas::presentation
