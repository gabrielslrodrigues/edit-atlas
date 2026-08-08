#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace edit_atlas::core {
namespace {

[[nodiscard]] FrameRate Rate(void) {
    return FrameRate::Create(24, 1).value();
}

[[nodiscard]] Timecode TimecodeAt(std::int64_t seconds) {
    return Timecode::Create(0, 0, seconds, 0, Rate(),
                            TimecodeMode::kNonDropFrame)
        .value();
}

[[nodiscard]] TimecodeRange Range(std::int64_t start_seconds,
                                  std::int64_t end_seconds) {
    return TimecodeRange::Create(TimecodeAt(start_seconds),
                                 TimecodeAt(end_seconds))
        .value();
}

TEST(EditorialTimelineTest, HoldsFormatIndependentEditorialData) {
    const SourceLineProvenance provenance{
        .location =
            {
                .source = "example.edl",
                .line = 3,
                .column = 1,
            },
        .line = "001  AX V C 00:00:00:00 00:00:01:00",
    };
    const EditEvent event{
        .identifier = "001",
        .reel = "AX",
        .track = {.kind = TrackKind::kVideo, .identifier = "V"},
        .edit_type = EditType::kCut,
        .transition = std::nullopt,
        .source_range = Range(0, 1),
        .record_range = Range(10, 11),
        .comments = {{.text = "FROM CLIP NAME: sunrise.mov",
                      .provenance = provenance}},
        .metadata =
            {
                {.key = "clip_name", .value = std::string{"sunrise.mov"}},
                {.key = "enabled", .value = true},
            },
        .provenance = provenance,
    };
    const TimelineDocument document{
        .title = "Example",
        .frame_rate = Rate(),
        .timecode_mode = TimecodeMode::kNonDropFrame,
        .events = {event},
        .metadata = {{.key = "format_hint", .value = std::string{"edl"}}},
        .diagnostics =
            {
                {
                    .severity = DiagnosticSeverity::kWarning,
                    .code = "example.warning",
                    .message = "Example diagnostic",
                    .location = provenance.location,
                },
            },
        .provenance = provenance,
    };

    ASSERT_EQ(document.events.size(), 1);
    EXPECT_EQ(document.events.front().reel, "AX");
    EXPECT_EQ(document.events.front().source_range.DurationInFrames(), 24);
    EXPECT_EQ(document.events.front().track.kind, TrackKind::kVideo);
    ASSERT_EQ(document.events.front().comments.size(), 1);
    EXPECT_EQ(document.events.front().comments.front().provenance, provenance);
    ASSERT_EQ(document.diagnostics.size(), 1);
    EXPECT_EQ(document.diagnostics.front().severity,
              DiagnosticSeverity::kWarning);
}

TEST(EditorialTimelineTest, RepresentsTransitionsAndTypedMetadata) {
    const EditEvent event{
        .identifier = "002",
        .reel = "BROLL",
        .track = {.kind = TrackKind::kAudio, .identifier = "A1"},
        .edit_type = EditType::kDissolve,
        .transition =
            Transition{
                .identifier = "D",
                .duration_frames = 12,
            },
        .source_range = Range(1, 2),
        .record_range = Range(11, 12),
        .comments = {},
        .metadata =
            {
                {.key = "channel", .value = std::int64_t{1}},
                {.key = "gain", .value = 0.75},
            },
        .provenance = std::nullopt,
    };

    ASSERT_TRUE(event.transition.has_value());
    EXPECT_EQ(event.transition->duration_frames, 12);
    EXPECT_TRUE(
        std::holds_alternative<std::int64_t>(event.metadata.front().value));
    EXPECT_TRUE(std::holds_alternative<double>(event.metadata.back().value));
}

} // namespace
} // namespace edit_atlas::core
