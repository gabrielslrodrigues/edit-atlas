#include <edit_atlas/core/timeline_projection.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

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

} // namespace
} // namespace edit_atlas::core
