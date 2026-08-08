#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace edit_atlas::services {
namespace {

[[nodiscard]] core::EditEvent
Event(std::string identifier, std::string reel, core::TrackKind track_kind,
      std::string track_identifier, core::EditType edit_type,
      std::string clip_name, std::string comment, std::int64_t start_frame) {
    const auto rate = core::FrameRate::Create(24, 1).value();
    const auto source_start =
        core::Timecode::FromFrameCount(start_frame, rate,
                                       core::TimecodeMode::kNonDropFrame)
            .value();
    const auto source_end =
        core::Timecode::FromFrameCount(start_frame + 24, rate,
                                       core::TimecodeMode::kNonDropFrame)
            .value();
    const auto record_start =
        core::Timecode::FromFrameCount(start_frame + 86'400, rate,
                                       core::TimecodeMode::kNonDropFrame)
            .value();
    const auto record_end =
        core::Timecode::FromFrameCount(start_frame + 86'424, rate,
                                       core::TimecodeMode::kNonDropFrame)
            .value();
    return core::EditEvent{
        .identifier = std::move(identifier),
        .reel = std::move(reel),
        .track =
            core::Track{
                .kind = track_kind,
                .identifier = std::move(track_identifier),
            },
        .edit_type = edit_type,
        .transition = std::nullopt,
        .source_range =
            core::TimecodeRange::Create(source_start, source_end).value(),
        .record_range =
            core::TimecodeRange::Create(record_start, record_end).value(),
        .comments =
            {
                core::Comment{
                    .text = std::move(comment),
                    .provenance = std::nullopt,
                },
            },
        .metadata =
            {
                core::MetadataEntry{
                    .key = "clip_name",
                    .value = std::move(clip_name),
                },
            },
        .provenance = std::nullopt,
    };
}

[[nodiscard]] core::TimelineDocument Document(void) {
    const auto rate = core::FrameRate::Create(24, 1).value();
    return core::TimelineDocument{
        .title = "FILTER TEST",
        .frame_rate = rate,
        .timecode_mode = core::TimecodeMode::kNonDropFrame,
        .events =
            {
                Event("001", "AX", core::TrackKind::kVideo, "V",
                      core::EditType::kCut, "Wide Shot", "Opening scene", 0),
                Event("002", "BX", core::TrackKind::kAudio, "A2",
                      core::EditType::kDissolve, "Hero Closeup",
                      "Use alternate take", 48),
                Event("003", "AX", core::TrackKind::kVideo, "V2",
                      core::EditType::kWipe, "Insert", "Ação final closeup",
                      96),
            },
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
}

class EventListExporter final : public core::Exporter {
  public:
    [[nodiscard]] const core::FormatDescriptor &
    descriptor(void) const noexcept override {
        return descriptor_;
    }

    [[nodiscard]] core::ExportResult
    Export(const core::ExportRequest &request) const override {
        std::string text;
        for (const auto &event : request.document.events) {
            text += event.identifier;
            text += '\n';
        }
        std::vector<std::byte> content;
        content.reserve(text.size());
        for (const auto character : text) {
            content.emplace_back(static_cast<std::byte>(character));
        }
        return core::ExportResult{
            .artifact =
                core::ExportArtifact{
                    .content = std::move(content),
                    .suggested_extension = "txt",
                    .media_type = "text/plain",
                },
            .diagnostics = {},
        };
    }

  private:
    core::FormatDescriptor descriptor_{
        .identifier = "event-list",
        .display_name = "Event list",
        .extensions = {"txt"},
    };
};

class TemporaryDestination final {
  public:
    TemporaryDestination(void)
        : path_{std::filesystem::path{testing::TempDir()} /
                "timeline-filter-export.txt"} {
        Remove();
    }

    ~TemporaryDestination(void) { Remove(); }

    TemporaryDestination(const TemporaryDestination &) = delete;
    TemporaryDestination &operator=(const TemporaryDestination &) = delete;
    TemporaryDestination(TemporaryDestination &&) = delete;
    TemporaryDestination &operator=(TemporaryDestination &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

  private:
    void Remove(void) const {
        std::error_code error;
        static_cast<void>(std::filesystem::remove(path_, error));
    }

    std::filesystem::path path_;
};

[[nodiscard]] std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
}

TEST(TimelineFilterTest, MatchesTextInTheSelectedField) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kClip,
                    .text = "CLOSEUP",
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = false,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{1}));
}

TEST(TimelineFilterTest, RestrictsConditionsToTheirSelectedFields) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kReel,
                    .text = "ax",
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = false,
                },
                TimelineEditTypeFilterCondition{
                    .edit_type = core::EditType::kWipe,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{2}));
}

TEST(TimelineFilterTest, CombinesConditionsUsingAnyMatching) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAny,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kEventIdentifier,
                    .text = "001",
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = false,
                },
                TimelineTrackKindFilterCondition{
                    .track_kind = core::TrackKind::kAudio,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{0, 1}));
}

TEST(TimelineFilterTest, SelectsAllEventsForAnEmptyQuery) {
    const auto document = Document();

    const auto result = FilterTimelineEvents(document, TimelineFilterQuery{});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{0, 1, 2}));
}

TEST(TimelineFilterTest, SupportsAnEmptyResult) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kClip,
                    .text = "missing",
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = false,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(TimelineFilterTest, MatchesCaseOnlyWhenRequested) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kClip,
                    .text = "hero",
                    .match_case = true,
                    .match_whole_word = false,
                    .regular_expression = false,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(TimelineFilterTest, SelectsTrackKindsWithoutTextMatching) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTrackKindFilterCondition{
                    .track_kind = core::TrackKind::kVideo,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{0, 2}));
}

TEST(TimelineFilterTest, SelectsEditTypesWithoutTextMatching) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineEditTypeFilterCondition{
                    .edit_type = core::EditType::kCut,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{0}));
}

TEST(TimelineFilterTest, SelectsExactTimecodes) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTimecodeFilterCondition{
                    .field = TimelineTimecodeFilterField::kSourceIn,
                    .timecode = "00:00:02:00",
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{1}));
}

TEST(TimelineFilterTest, SelectsExactDurations) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineDurationFilterCondition{
                    .frames = 24,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{0, 1, 2}));
}

TEST(TimelineFilterTest, MatchesOnlyCompleteWordsWhenRequested) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kClip,
                    .text = "close",
                    .match_case = false,
                    .match_whole_word = true,
                    .regular_expression = false,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(TimelineFilterTest, AppliesCaseInsensitiveWholeWordsToUtf8Text) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kComments,
                    .text = "ação",
                    .match_case = false,
                    .match_whole_word = true,
                    .regular_expression = false,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{2}));
}

TEST(TimelineFilterTest, EvaluatesRegularExpressions) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kEventIdentifier,
                    .text = R"(^00[13]$)",
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = true,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, (TimelineEventSelection{0, 2}));
}

TEST(TimelineFilterTest, ReportsInvalidRegularExpressions) {
    const auto document = Document();
    const TimelineFilterQuery query{
        .combination = TimelineFilterCombination::kAll,
        .conditions =
            {
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kClip,
                    .text = {},
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = false,
                },
                TimelineTextFilterCondition{
                    .field = TimelineTextFilterField::kComments,
                    .text = "(",
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = true,
                },
            },
    };

    const auto result = FilterTimelineEvents(document, query);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().condition_index, 1);
    EXPECT_FALSE(result.error().message.empty());
}

TEST(TimelineFilterTest, CopiesOnlySelectedEventsWithoutChangingTheSource) {
    const auto document = Document();
    constexpr std::array<std::size_t, 2> kSelection{2, 0};

    const auto selected = SelectTimelineEvents(document, kSelection);

    ASSERT_EQ(selected.events.size(), 2);
    EXPECT_EQ(selected.events[0].identifier, "003");
    EXPECT_EQ(selected.events[1].identifier, "001");
    EXPECT_EQ(document.events.size(), 3);
}

TEST(TimelineFilterTest, ExportsOnlyTheFilteredSelection) {
    core::FormatRegistry registry;
    ASSERT_TRUE(registry.RegisterExporter(std::make_unique<EventListExporter>())
                    .has_value());
    const TimelineDocumentExportService export_service{registry};
    const TemporaryDestination destination;
    const auto document = Document();
    const auto selection = FilterTimelineEvents(
        document, TimelineFilterQuery{
                      .combination = TimelineFilterCombination::kAll,
                      .conditions =
                          {
                              TimelineTextFilterCondition{
                                  .field = TimelineTextFilterField::kReel,
                                  .text = "bx",
                                  .match_case = false,
                                  .match_whole_word = false,
                                  .regular_expression = false,
                              },
                          },
                  });

    ASSERT_TRUE(selection.has_value());
    const auto result = export_service.Export(TimelineDocumentExportRequest{
        .path = destination.path(),
        .format_identifier = "event-list",
        .timeline = SelectTimelineEvents(document, *selection),
        .event_projection =
            {
                core::DefaultTimelineEventProjection().begin(),
                core::DefaultTimelineEventProjection().end(),
            },
        .options = {},
        .replace_existing = false,
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(ReadFile(destination.path()), "002\n");
}

} // namespace
} // namespace edit_atlas::services
