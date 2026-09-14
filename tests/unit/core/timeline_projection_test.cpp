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

#include "edit_atlas/core/timeline_projection.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "gtest/gtest.h"

namespace edit_atlas::core {
namespace {

TEST(TimelineProjectionTest, ProvidesStableIdentifiersAndDefaultOrder) {
  const auto projection = DefaultTimelineEventProjection();
  constexpr std::array<std::string_view, 17> kIdentifiers{
      "event",           "reel",       "track-kind",        "track",
      "edit-type",       "transition", "transition-frames", "source-in",
      "source-out",      "record-in",  "record-out",        "duration",
      "duration-frames", "clip-name",  "source-file",       "comments",
      "source-line",
  };

  ASSERT_EQ(projection.size(), kIdentifiers.size());
  EXPECT_EQ(projection.front(), TimelineEventField::kEventIdentifier);
  EXPECT_EQ(projection.back(), TimelineEventField::kSourceLine);
  for (std::size_t index = 0; index < projection.size(); ++index) {
    EXPECT_EQ(TimelineEventFieldIdentifier(projection[index]),
              kIdentifiers[index]);
    EXPECT_EQ(TimelineEventFieldFromIdentifier(kIdentifiers[index]),
              projection[index]);
  }
  EXPECT_EQ(TimelineEventFieldFromIdentifier("Track type"), std::nullopt);
  EXPECT_TRUE(IsValidTimelineEventProjection(projection));

  const auto fields = TimelineEventFields();
  ASSERT_EQ(fields.size(), kTimelineEventFieldCount);
  EXPECT_EQ(fields[1], TimelineEventField::kInitialFrame);
  EXPECT_EQ(TimelineEventFieldIdentifier(fields[1]), "initial-frame");
  EXPECT_EQ(TimelineEventFieldFromIdentifier("initial-frame"), fields[1]);
  EXPECT_EQ(std::ranges::find(projection, TimelineEventField::kInitialFrame),
            projection.end());
}

TEST(TimelineProjectionTest, RejectsEmptyAndDuplicateProjections) {
  constexpr std::array<TimelineEventField, 0> kEmpty{};
  constexpr std::array kDuplicate{
      TimelineEventField::kReel,
      TimelineEventField::kReel,
  };
  constexpr std::array kSubset{
      TimelineEventField::kComments,
      TimelineEventField::kEventIdentifier,
  };

  EXPECT_FALSE(IsValidTimelineEventProjection(kEmpty));
  EXPECT_FALSE(IsValidTimelineEventProjection(kDuplicate));
  EXPECT_TRUE(IsValidTimelineEventProjection(kSubset));
}

}  // namespace
}  // namespace edit_atlas::core
