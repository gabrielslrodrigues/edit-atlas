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

#include "edit_atlas/presentation/timeline_event_projection_model.hpp"

#include <array>
#include <vector>

#include "QString"
#include "gtest/gtest.h"

#include "edit_atlas/core/timeline_projection.hpp"

namespace edit_atlas::presentation {
namespace {

TEST(TimelineEventProjectionModelTest, PresentsSelectedFieldsBeforeChoices) {
  TimelineEventProjectionModel model;
  const std::array projection{
      core::TimelineEventField::kClipName,
      core::TimelineEventField::kEventIdentifier,
  };

  ASSERT_TRUE(model.SetProjection(projection));

  EXPECT_EQ(model.rowCount(), static_cast<int>(core::kTimelineEventFieldCount));
  EXPECT_EQ(model.SelectedCount(), 2);
  EXPECT_TRUE(model.IsValid());
  EXPECT_EQ(model.data(model.index(0, 0)).toString(),
            QStringLiteral("Clip name"));
  EXPECT_EQ(model
                .data(model.index(0, 0),
                      TimelineEventProjectionModel::kIdentifierRole)
                .toString(),
            QStringLiteral("clip-name"));
  EXPECT_TRUE(
      model.data(model.index(0, 0), TimelineEventProjectionModel::kSelectedRole)
          .toBool());
  EXPECT_EQ(model.Projection(), std::vector<core::TimelineEventField>(
                                    projection.begin(), projection.end()));
}

TEST(TimelineEventProjectionModelTest, EditsAndReordersTheProjection) {
  TimelineEventProjectionModel model;
  const std::array projection{
      core::TimelineEventField::kEventIdentifier,
      core::TimelineEventField::kReel,
  };
  ASSERT_TRUE(model.SetProjection(projection));

  model.MoveDown(0);
  model.Move(1, 3);
  EXPECT_EQ(
      model.data(model.index(3, 0), TimelineEventProjectionModel::kFieldRole)
          .toInt(),
      static_cast<int>(core::TimelineEventField::kEventIdentifier));
  model.Move(3, 1);
  model.SetSelected(1, false);

  ASSERT_EQ(model.SelectedCount(), 1);
  EXPECT_EQ(model.Projection(), std::vector{core::TimelineEventField::kReel});
  model.SetSelected(0, false);
  EXPECT_FALSE(model.IsValid());
  EXPECT_TRUE(model.Projection().empty());
}

TEST(TimelineEventProjectionModelTest, RejectsUnknownOrDuplicateFields) {
  TimelineEventProjectionModel model;
  const auto original = model.Projection();
  const std::array duplicate{
      core::TimelineEventField::kReel,
      core::TimelineEventField::kReel,
  };
  const std::array unknown{
      core::TimelineEventField::kCount,
  };

  EXPECT_FALSE(model.SetProjection(duplicate));
  EXPECT_FALSE(model.SetProjection(unknown));
  EXPECT_EQ(model.Projection(), original);
}

}  // namespace
}  // namespace edit_atlas::presentation
