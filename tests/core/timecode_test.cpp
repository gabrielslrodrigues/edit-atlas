#include <edit_atlas/core/timecode.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>

namespace edit_atlas::core {
namespace {

[[nodiscard]] FrameRate Rate(std::int64_t numerator, std::int64_t denominator) {
    return FrameRate::Create(numerator, denominator).value();
}

[[nodiscard]] Timecode
MakeTimecode(std::int64_t hours, std::int64_t minutes, std::int64_t seconds,
             std::int64_t frames, const FrameRate &rate,
             TimecodeMode mode = TimecodeMode::kNonDropFrame) {
    return Timecode::Create(hours, minutes, seconds, frames, rate, mode)
        .value();
}

TEST(FrameRateTest, NormalizesRationalValues) {
    const auto rate = FrameRate::Create(48'000, 2'002);

    ASSERT_TRUE(rate.has_value());
    EXPECT_EQ(rate->numerator(), 24'000);
    EXPECT_EQ(rate->denominator(), 1'001);
}

TEST(FrameRateTest, RejectsInvalidValues) {
    EXPECT_EQ(FrameRate::Create(0, 1).error(),
              FrameRateError::kNonPositiveNumerator);
    EXPECT_EQ(FrameRate::Create(-24, 1).error(),
              FrameRateError::kNonPositiveNumerator);
    EXPECT_EQ(FrameRate::Create(24, 0).error(),
              FrameRateError::kNonPositiveDenominator);
    EXPECT_EQ(FrameRate::Create(24, -1).error(),
              FrameRateError::kNonPositiveDenominator);
    EXPECT_EQ(FrameRate::Create(static_cast<std::int64_t>(
                                    std::numeric_limits<std::int32_t>::max()) +
                                    1,
                                1)
                  .error(),
              FrameRateError::kValueOutOfRange);
}

TEST(TimecodeTest, SupportsCommonEditorialRatesWithoutDrift) {
    constexpr std::array<std::pair<std::int64_t, std::int64_t>, 7> kRates{{
        {24, 1},
        {25, 1},
        {30, 1},
        {60, 1},
        {24'000, 1'001},
        {30'000, 1'001},
        {60'000, 1'001},
    }};

    for (const auto &[numerator, denominator] : kRates) {
        const auto rate = Rate(numerator, denominator);
        const auto timecode = MakeTimecode(12, 34, 56, 12, rate);
        const auto restored = Timecode::FromFrameCount(
            timecode.ToFrameCount(), rate, TimecodeMode::kNonDropFrame);

        ASSERT_TRUE(restored.has_value());
        EXPECT_EQ(*restored, timecode);
    }
}

TEST(TimecodeTest, ConvertsNonDropTimecodeToFrameCount) {
    const auto timecode = MakeTimecode(1, 2, 3, 4, Rate(25, 1));

    EXPECT_EQ(timecode.ToFrameCount(), 93'079);
}

TEST(TimecodeTest, RejectsInvalidComponents) {
    const auto rate = Rate(25, 1);

    EXPECT_EQ(Timecode::Create(24, 0, 0, 0, rate, TimecodeMode::kNonDropFrame)
                  .error(),
              TimecodeError::kHourOutOfRange);
    EXPECT_EQ(Timecode::Create(0, 60, 0, 0, rate, TimecodeMode::kNonDropFrame)
                  .error(),
              TimecodeError::kMinuteOutOfRange);
    EXPECT_EQ(Timecode::Create(0, 0, 60, 0, rate, TimecodeMode::kNonDropFrame)
                  .error(),
              TimecodeError::kSecondOutOfRange);
    EXPECT_EQ(Timecode::Create(0, 0, 0, 25, rate, TimecodeMode::kNonDropFrame)
                  .error(),
              TimecodeError::kFrameOutOfRange);
}

TEST(TimecodeTest, RejectsUnsupportedDropFrameRates) {
    const auto result =
        Timecode::Create(0, 0, 0, 0, Rate(25, 1), TimecodeMode::kDropFrame);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TimecodeError::kUnsupportedDropFrameRate);
}

TEST(TimecodeTest, EnforcesTwentyNineNinetySevenDropFrameLabels) {
    const auto rate = Rate(30'000, 1'001);

    EXPECT_EQ(
        Timecode::Create(0, 1, 0, 0, rate, TimecodeMode::kDropFrame).error(),
        TimecodeError::kDroppedFrameLabel);
    EXPECT_EQ(
        Timecode::Create(0, 1, 0, 1, rate, TimecodeMode::kDropFrame).error(),
        TimecodeError::kDroppedFrameLabel);
    EXPECT_TRUE(Timecode::Create(0, 1, 0, 2, rate, TimecodeMode::kDropFrame));
    EXPECT_TRUE(Timecode::Create(0, 10, 0, 0, rate, TimecodeMode::kDropFrame));
}

TEST(TimecodeTest, ConvertsAcrossDropFrameMinuteBoundaries) {
    const auto rate = Rate(30'000, 1'001);

    const auto before_boundary =
        Timecode::FromFrameCount(1'799, rate, TimecodeMode::kDropFrame);
    const auto after_boundary =
        Timecode::FromFrameCount(1'800, rate, TimecodeMode::kDropFrame);
    const auto ten_minutes =
        Timecode::FromFrameCount(17'982, rate, TimecodeMode::kDropFrame);

    ASSERT_TRUE(before_boundary.has_value());
    EXPECT_EQ(*before_boundary,
              MakeTimecode(0, 0, 59, 29, rate, TimecodeMode::kDropFrame));
    ASSERT_TRUE(after_boundary.has_value());
    EXPECT_EQ(*after_boundary,
              MakeTimecode(0, 1, 0, 2, rate, TimecodeMode::kDropFrame));
    ASSERT_TRUE(ten_minutes.has_value());
    EXPECT_EQ(*ten_minutes,
              MakeTimecode(0, 10, 0, 0, rate, TimecodeMode::kDropFrame));
}

TEST(TimecodeTest, EnforcesFiftyNineNinetyFourDropFrameLabels) {
    const auto rate = Rate(60'000, 1'001);

    for (std::int64_t frame = 0; frame < 4; ++frame) {
        const auto result =
            Timecode::Create(0, 1, 0, frame, rate, TimecodeMode::kDropFrame);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), TimecodeError::kDroppedFrameLabel);
    }

    EXPECT_TRUE(Timecode::Create(0, 1, 0, 4, rate, TimecodeMode::kDropFrame));
}

TEST(TimecodeTest, RejectsFrameCountsOutsideOneDay) {
    const auto rate = Rate(25, 1);

    EXPECT_EQ(
        Timecode::FromFrameCount(-1, rate, TimecodeMode::kNonDropFrame).error(),
        TimecodeError::kFrameCountOutOfRange);
    EXPECT_EQ(Timecode::FromFrameCount(25 * 60 * 60 * 24, rate,
                                       TimecodeMode::kNonDropFrame)
                  .error(),
              TimecodeError::kFrameCountOutOfRange);
}

TEST(TimecodeRangeTest, ComputesHalfOpenDuration) {
    const auto rate = Rate(24, 1);
    const auto range = TimecodeRange::Create(MakeTimecode(0, 0, 1, 0, rate),
                                             MakeTimecode(0, 0, 2, 12, rate));

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->DurationInFrames(), 36);
}

TEST(TimecodeRangeTest, FormatsNonDropFrameDurationsWithoutHourWrapping) {
    const auto rate = Rate(24, 1);
    const auto range = TimecodeRange::Create(MakeTimecode(0, 0, 0, 0, rate),
                                             MakeTimecode(1, 2, 3, 4, rate));

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->Duration(), "01:02:03:04");
}

TEST(TimecodeRangeTest, FormatsDropFrameDurationsUsingDropFrameNumbering) {
    const auto rate = Rate(30'000, 1'001);
    const auto range = TimecodeRange::Create(
        MakeTimecode(0, 0, 0, 0, rate, TimecodeMode::kDropFrame),
        MakeTimecode(1, 0, 0, 0, rate, TimecodeMode::kDropFrame));

    ASSERT_TRUE(range.has_value());
    EXPECT_EQ(range->Duration(), "01:00:00;00");
}

TEST(TimecodeRangeTest, RejectsMismatchedRatesAndModes) {
    const auto rate_24 = Rate(24, 1);
    const auto rate_25 = Rate(25, 1);
    const auto ntsc_rate = Rate(30'000, 1'001);

    EXPECT_EQ(TimecodeRange::Create(MakeTimecode(0, 0, 0, 0, rate_24),
                                    MakeTimecode(0, 0, 0, 0, rate_25))
                  .error(),
              TimecodeRangeError::kMismatchedFrameRate);
    EXPECT_EQ(TimecodeRange::Create(
                  MakeTimecode(0, 0, 0, 0, ntsc_rate),
                  MakeTimecode(0, 0, 0, 0, ntsc_rate, TimecodeMode::kDropFrame))
                  .error(),
              TimecodeRangeError::kMismatchedMode);
}

TEST(TimecodeRangeTest, RejectsAnEndBeforeItsStart) {
    const auto rate = Rate(24, 1);
    const auto range = TimecodeRange::Create(MakeTimecode(0, 0, 2, 0, rate),
                                             MakeTimecode(0, 0, 1, 23, rate));

    ASSERT_FALSE(range.has_value());
    EXPECT_EQ(range.error(), TimecodeRangeError::kEndBeforeStart);
}

} // namespace
} // namespace edit_atlas::core
