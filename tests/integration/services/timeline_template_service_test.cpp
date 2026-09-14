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

#include "edit_atlas/services/timeline_template_service.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "edit_atlas/core/editorial_timeline.hpp"
#include "edit_atlas/core/timeline_projection.hpp"
#include "edit_atlas/services/timeline_filter.hpp"
#include "edit_atlas/services/timeline_template.hpp"

namespace edit_atlas::services {
namespace {

class TemporaryTemplateDirectory final {
 public:
  TemporaryTemplateDirectory(void)
      : path_{
            std::filesystem::path{testing::TempDir()} /
            ("edit-atlas-templates-" + GenerateTimelineTemplateIdentifier())} {
    std::filesystem::create_directories(path_);
  }

  ~TemporaryTemplateDirectory(void) {
    std::error_code error;
    static_cast<void>(std::filesystem::remove_all(path_, error));
  }

  TemporaryTemplateDirectory(const TemporaryTemplateDirectory&) = delete;
  TemporaryTemplateDirectory& operator=(const TemporaryTemplateDirectory&) =
      delete;
  TemporaryTemplateDirectory(TemporaryTemplateDirectory&&) = delete;
  TemporaryTemplateDirectory& operator=(TemporaryTemplateDirectory&&) = delete;

  [[nodiscard]] const std::filesystem::path& path(void) const noexcept {
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

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{input},
          std::istreambuf_iterator<char>{}};
}

void WriteFile(const std::filesystem::path& path, std::string_view content) {
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
  const auto created =
      service.Create(original.name, original.filter, original.event_projection);
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
  const auto created =
      service.Create(original.name, original.filter, original.event_projection);
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

TEST(TimelineTemplateServiceTest,
     RejectsMalformedFilesWithoutLosingNeighbours) {
  const TemporaryTemplateDirectory directory;
  TimelineTemplateService service{directory.path()};
  ASSERT_TRUE(service.Load().has_value());
  const auto original = Template();
  const auto created =
      service.Create(original.name, original.filter, original.event_projection);
  ASSERT_TRUE(created.has_value());

  const std::string valid =
      R"({"schema_version":1,"identifier":"invalid","name":"Invalid",)"
      R"("filter":{"combination":"all","conditions":[])"
      R"(},"event_columns":["event"]})";
  const std::vector<std::pair<std::string, std::string>> replacements{
      {"\"schema_version\":1", "\"schema_version\":1.0"},
      {"\"schema_version\":1", "\"schema_version\":4294967297"},
      {"\"schema_version\":1", "\"schema_version\":18446744073709551615"},
      {"\"schema_version\":1", "\"schema_version\":\"1\""},
      {"\"schema_version\":1", "\"schema_version\":true"},
      {"\"schema_version\":1,", ""},
      {"\"identifier\":\"invalid\"", "\"identifier\":\"another\""},
      {"\"identifier\":\"invalid\"", "\"identifier\":\"../invalid\""},
      {"\"name\":\"Invalid\"", "\"name\":\"\""},
      {"\"name\":\"Invalid\"", "\"name\":null"},
      {"\"combination\":\"all\"", "\"combination\":\"unknown\""},
      {"\"combination\":\"all\"", "\"combination\":false"},
      {"\"conditions\":[]", "\"conditions\":{}"},
      {"\"conditions\":[]", "\"conditions\":null"},
      {"\"conditions\":[]", "\"conditions\":[null]"},
      {"\"conditions\":[]", R"("conditions":[{"type":"unknown"}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"duration","frames":1.5}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"duration","frames":true}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"duration","frames":9223372036854775808}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"duration","frames":-9223372036854775809}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"duration","frames":0,"future":true}])"},
      {"\"conditions\":[]", R"("conditions":[{"type":"duration"}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"track-kind","value":"unknown"}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"edit-type","value":"unknown"}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"timecode","field":"unknown","value":""}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"timecode","field":"source-in","value":0}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"text","field":"unknown","text":"",)"
       R"("match_case":false,"match_whole_word":false,)"
       R"("regular_expression":false}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"text","field":"event","text":"",)"
       R"("match_case":0,"match_whole_word":false,)"
       R"("regular_expression":false}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"text","field":"event","text":"",)"
       R"("match_case":false,"match_whole_word":"false",)"
       R"("regular_expression":false}])"},
      {"\"conditions\":[]",
       R"("conditions":[{"type":"text","field":"event","text":"",)"
       R"("match_case":false,"match_whole_word":false,)"
       R"("regular_expression":null}])"},
      {"\"event_columns\":[\"event\"]", "\"event_columns\":{}"},
      {"\"event_columns\":[\"event\"]", "\"event_columns\":[1]"},
      {"\"event_columns\":[\"event\"]",
       "\"event_columns\":[\"event\",\"event\"]"},
      {"\"event_columns\":[\"event\"]", "\"event_columns\":[]"},
      {valid, "{"},
      {valid, "[]"},
      {valid, "null"},
  };
  const auto path = directory.path() / "invalid.json";
  for (const auto& [before, after] : replacements) {
    SCOPED_TRACE(after);
    auto content = valid;
    const auto position = content.find(before);
    ASSERT_NE(position, std::string::npos);
    content.replace(position, before.size(), after);
    WriteFile(path, content);

    TimelineTemplateService restored{directory.path()};
    const auto result = restored.Load();
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 1);
    EXPECT_EQ(result->front().path, path);
    EXPECT_FALSE(result->front().message.empty());
    ASSERT_EQ(restored.Templates().size(), 1);
    EXPECT_EQ(restored.Templates().front(), *created);
  }
}

TEST(TimelineTemplateServiceTest, KeepsSavedTemplateWhenSerializationFails) {
  const TemporaryTemplateDirectory directory;
  TimelineTemplateService service{directory.path()};
  ASSERT_TRUE(service.Load().has_value());
  const auto original = Template();
  const auto created =
      service.Create(original.name, original.filter, original.event_projection);
  ASSERT_TRUE(created.has_value());
  const auto path = directory.path() / (created->identifier + ".json");
  const auto before = ReadFile(path);
  auto invalid = original.filter;
  invalid.combination = static_cast<TimelineFilterCombination>(-1);
  std::vector<TimelineFilterQuery> invalid_queries{invalid};
  const std::vector<TimelineFilterCondition> invalid_conditions{
      TimelineTextFilterCondition{
          .field = static_cast<TimelineTextFilterField>(-1),
          .text = "",
          .match_case = false,
          .match_whole_word = false,
          .regular_expression = false,
      },
      TimelineTrackKindFilterCondition{.track_kind =
                                           static_cast<core::TrackKind>(-1)},
      TimelineEditTypeFilterCondition{.edit_type =
                                          static_cast<core::EditType>(-1)},
      TimelineTimecodeFilterCondition{
          .field = static_cast<TimelineTimecodeFilterField>(-1),
          .timecode = "",
      },
  };
  for (const auto& condition : invalid_conditions) {
    invalid = original.filter;
    invalid.conditions = {condition};
    invalid_queries.push_back(invalid);
  }

  for (const auto& query : invalid_queries) {
    const auto result =
        service.Update(created->identifier, query, original.event_projection);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, TimelineTemplateFailureKind::kStorageFailed);
    EXPECT_EQ(ReadFile(path), before);
    ASSERT_NE(service.Find(created->identifier), nullptr);
    EXPECT_EQ(*service.Find(created->identifier), *created);
  }
}

TEST(TimelineTemplateServiceTest,
     KeepsCatalogueWhenDestinationCannotBeCommitted) {
  const TemporaryTemplateDirectory directory;
  TimelineTemplateService service{directory.path()};
  ASSERT_TRUE(service.Load().has_value());
  const auto original = Template();
  const auto created =
      service.Create(original.name, original.filter, original.event_projection);
  ASSERT_TRUE(created.has_value());
  const auto destination = directory.path() / (created->identifier + ".json");
  ASSERT_TRUE(std::filesystem::remove(destination));
  ASSERT_TRUE(std::filesystem::create_directory(destination));
  WriteFile(destination / "occupied", "preserve");

  const auto result = service.Rename(created->identifier, "Changed name");

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, TimelineTemplateFailureKind::kStorageFailed);
  EXPECT_EQ(ReadFile(destination / "occupied"), "preserve");
  ASSERT_NE(service.Find(created->identifier), nullptr);
  EXPECT_EQ(*service.Find(created->identifier), *created);
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
  EXPECT_EQ(duplicate.error().kind, TimelineTemplateFailureKind::kNameConflict);
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

}  // namespace
}  // namespace edit_atlas::services
