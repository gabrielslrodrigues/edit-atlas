#include <edit_atlas/services/timeline_template.hpp>
#include <edit_atlas/services/timeline_template_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_filter.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace edit_atlas::services {
namespace {

class TemporaryTemplateDirectory final {
  public:
    TemporaryTemplateDirectory(void)
        : path_{std::filesystem::path{testing::TempDir()} /
                ("edit-atlas-templates-" +
                 GenerateTimelineTemplateIdentifier())} {
        std::filesystem::create_directories(path_);
    }

    ~TemporaryTemplateDirectory(void) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove_all(path_, error));
    }

    TemporaryTemplateDirectory(const TemporaryTemplateDirectory &) = delete;
    TemporaryTemplateDirectory &
    operator=(const TemporaryTemplateDirectory &) = delete;
    TemporaryTemplateDirectory(TemporaryTemplateDirectory &&) = delete;
    TemporaryTemplateDirectory &
    operator=(TemporaryTemplateDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] TimelineTemplate Template(void) {
    return TimelineTemplate{
        .identifier = "0123456789abcdef0123456789abcdef",
        .name = "Dialogue review",
        .filter =
            TimelineFilterQuery{
                .combination = TimelineFilterCombination::kAny,
                .conditions =
                    {
                        TimelineTextFilterCondition{
                            .field = TimelineTextFilterField::kComments,
                            .text = "dialogue",
                            .match_case = true,
                            .match_whole_word = true,
                            .regular_expression = false,
                        },
                        TimelineTrackKindFilterCondition{
                            .track_kind = core::TrackKind::kAudio,
                        },
                        TimelineEditTypeFilterCondition{
                            .edit_type = core::EditType::kDissolve,
                        },
                        TimelineTimecodeFilterCondition{
                            .field = TimelineTimecodeFilterField::kRecordIn,
                            .timecode = "01:00:00:00",
                        },
                        TimelineDurationFilterCondition{
                            .frames = 48,
                        },
                    },
            },
        .event_projection =
            {
                core::TimelineEventField::kComments,
                core::TimelineEventField::kEventIdentifier,
                core::TimelineEventField::kRecordIn,
            },
    };
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
}

void WriteFile(const std::filesystem::path &path, std::string_view content) {
    std::ofstream output{
        path,
        std::ios::binary | std::ios::out | std::ios::trunc,
    };
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

TEST(TimelineTemplateServiceTest, RoundTripsStableNonlocalizedValues) {
    const TemporaryTemplateDirectory directory;
    TimelineTemplateService service{directory.path()};
    const auto original = Template();

    ASSERT_TRUE(service.Load().has_value());
    const auto created = service.Create(original.name, original.filter,
                                        original.event_projection);
    ASSERT_TRUE(created.has_value());
    TimelineTemplateService restored{directory.path()};
    const auto result = restored.Load();

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(restored.Templates().size(), 1);
    EXPECT_EQ(restored.Templates().front(), *created);
    EXPECT_TRUE(result->empty());
    const auto persisted =
        ReadFile(directory.path() / (created->identifier + ".json"));
    EXPECT_NE(persisted.find("\"schema_version\": 1"), std::string::npos);
    EXPECT_NE(persisted.find("\"combination\": \"any\""), std::string::npos);
    EXPECT_NE(persisted.find("\"field\": \"comments\""), std::string::npos);
    EXPECT_NE(persisted.find("\"track-kind\""), std::string::npos);
    EXPECT_NE(persisted.find("\"frames\": 48"), std::string::npos);
    EXPECT_NE(persisted.find("\"event_columns\""), std::string::npos);
    EXPECT_NE(persisted.find("\"record-in\""), std::string::npos);
}

TEST(TimelineTemplateServiceTest, UpdatesRenamesDuplicatesAndRemoves) {
    const TemporaryTemplateDirectory directory;
    TimelineTemplateService service{directory.path()};
    ASSERT_TRUE(service.Load().has_value());
    const auto original = Template();
    const auto created = service.Create(original.name, original.filter,
                                        original.event_projection);
    ASSERT_TRUE(created.has_value());

    const auto renamed =
        service.Rename(created->identifier, "Updated dialogue review");
    ASSERT_TRUE(renamed.has_value());
    auto changed_filter = original.filter;
    changed_filter.conditions.clear();
    const auto updated = service.Update(created->identifier, changed_filter,
                                        {core::TimelineEventField::kClipName});
    ASSERT_TRUE(updated.has_value());
    const auto duplicate =
        service.Duplicate(created->identifier, "Dialogue copy");
    ASSERT_TRUE(duplicate.has_value());
    EXPECT_EQ(duplicate->filter, changed_filter);
    EXPECT_EQ(duplicate->event_projection,
              std::vector{core::TimelineEventField::kClipName});
    ASSERT_EQ(service.Templates().size(), 2);

    ASSERT_TRUE(service.Remove(created->identifier).has_value());
    EXPECT_EQ(service.Templates().size(), 1);
}

TEST(TimelineTemplateServiceTest, SkipsUnsupportedOrUnknownSchemaValues) {
    const TemporaryTemplateDirectory directory;
    TimelineTemplateService service{directory.path()};
    ASSERT_TRUE(service.Load().has_value());
    const auto valid = Template();
    ASSERT_TRUE(service.Create(valid.name, valid.filter, valid.event_projection)
                    .has_value());
    WriteFile(
        directory.path() / "future.json",
        R"({"schema_version":2,"identifier":"future","name":"Future","filter":{"combination":"all","conditions":[]},"event_columns":["event"]})");
    WriteFile(
        directory.path() / "unknown-column.json",
        R"({"schema_version":1,"identifier":"unknown-column","name":"Unknown","filter":{"combination":"all","conditions":[]},"event_columns":["future-field"]})");
    WriteFile(
        directory.path() / "unknown-field.json",
        R"({"schema_version":1,"identifier":"unknown-field","name":"Unknown","filter":{"combination":"all","conditions":[]},"event_columns":["event"],"future_behavior":true})");

    TimelineTemplateService restored{directory.path()};
    const auto result = restored.Load();

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(restored.Templates().size(), 1);
    EXPECT_EQ(result->size(), 3);
}

TEST(TimelineTemplateServiceTest, RejectsDuplicateNames) {
    const TemporaryTemplateDirectory directory;
    TimelineTemplateService service{directory.path()};
    ASSERT_TRUE(service.Load().has_value());
    const auto value = Template();
    ASSERT_TRUE(service.Create(value.name, value.filter, value.event_projection)
                    .has_value());

    const auto duplicate =
        service.Create(value.name, value.filter, value.event_projection);

    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error().kind,
              TimelineTemplateFailureKind::kNameConflict);
    EXPECT_TRUE(
        service.Create("dialogue review", value.filter, value.event_projection)
            .has_value());
}

TEST(TimelineTemplateTest, GeneratesFilesystemSafeIdentifiers) {
    const auto identifier = GenerateTimelineTemplateIdentifier();

    EXPECT_EQ(identifier.size(), 32);
    EXPECT_TRUE(std::ranges::all_of(identifier, [](unsigned char character) {
        return std::isdigit(character) != 0 ||
               (character >= 'a' && character <= 'f');
    }));
}

} // namespace
} // namespace edit_atlas::services
