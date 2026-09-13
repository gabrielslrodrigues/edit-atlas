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

#include "timeline_template_store.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "nlohmann/json.hpp"

#include "edit_atlas/core/editorial_timeline.hpp"
#include "edit_atlas/core/timeline_projection.hpp"
#include "edit_atlas/services/timeline_filter.hpp"
#include "edit_atlas/services/timeline_template.hpp"
#include "edit_atlas/storage/local_file.hpp"

namespace edit_atlas::services {
namespace {

using Json = nlohmann::json;

constexpr std::string_view kFileExtension = ".json";

[[nodiscard]] bool IsValidIdentifier(std::string_view identifier) {
  return !identifier.empty() &&
         std::ranges::all_of(identifier, [](char character) {
           return (character >= 'a' && character <= 'z') ||
                  (character >= '0' && character <= '9') || character == '-';
         });
}

struct ValidationFailure final {
  std::string message;
};

template <typename T>
using ValidationResult = std::expected<T, ValidationFailure>;

struct JsonField final {
  std::string_view name;
  Json::value_t type;
};

[[nodiscard]] ValidationResult<void> ValidateObject(
    const Json& value, std::initializer_list<JsonField> fields) {
  if (!value.is_object()) {
    return std::unexpected(ValidationFailure{"Expected a JSON object."});
  }
  for (const auto& item : value.items()) {
    if (std::ranges::find(fields, item.key(), &JsonField::name) ==
        fields.end()) {
      return std::unexpected(
          ValidationFailure{"Unknown field \"" + item.key() + "\"."});
    }
  }
  for (const auto& field : fields) {
    const auto found = value.find(field.name);
    if (found == value.end()) {
      return std::unexpected(ValidationFailure{
          "Missing field \"" + std::string{field.name} + "\"."});
    }
    // Integer fields accept either JSON integer representation without
    // narrowing.
    if (found->type() != field.type &&
        !(field.type == Json::value_t::number_integer &&
          found->is_number_integer())) {
      return std::unexpected(ValidationFailure{
          "Wrong type for field \"" + std::string{field.name} + "\"."});
    }
  }
  return {};
}

[[nodiscard]] std::string_view TextFieldIdentifier(
    TimelineTextFilterField field) noexcept {
  switch (field) {
    case TimelineTextFilterField::kEventIdentifier:
      return "event";
    case TimelineTextFilterField::kReel:
      return "reel";
    case TimelineTextFilterField::kTrackIdentifier:
      return "track";
    case TimelineTextFilterField::kClip:
      return "clip";
    case TimelineTextFilterField::kComments:
      return "comments";
  }
  return {};
}

[[nodiscard]] std::optional<TimelineTextFilterField> TextFieldFromIdentifier(
    std::string_view identifier) noexcept {
  if (identifier == "event") {
    return TimelineTextFilterField::kEventIdentifier;
  }
  if (identifier == "reel") {
    return TimelineTextFilterField::kReel;
  }
  if (identifier == "track") {
    return TimelineTextFilterField::kTrackIdentifier;
  }
  if (identifier == "clip") {
    return TimelineTextFilterField::kClip;
  }
  if (identifier == "comments") {
    return TimelineTextFilterField::kComments;
  }
  return std::nullopt;
}

[[nodiscard]] std::string_view TimecodeFieldIdentifier(
    TimelineTimecodeFilterField field) noexcept {
  switch (field) {
    case TimelineTimecodeFilterField::kSourceIn:
      return "source-in";
    case TimelineTimecodeFilterField::kSourceOut:
      return "source-out";
    case TimelineTimecodeFilterField::kRecordIn:
      return "record-in";
    case TimelineTimecodeFilterField::kRecordOut:
      return "record-out";
  }
  return {};
}

[[nodiscard]] std::optional<TimelineTimecodeFilterField>
TimecodeFieldFromIdentifier(std::string_view identifier) noexcept {
  if (identifier == "source-in") {
    return TimelineTimecodeFilterField::kSourceIn;
  }
  if (identifier == "source-out") {
    return TimelineTimecodeFilterField::kSourceOut;
  }
  if (identifier == "record-in") {
    return TimelineTimecodeFilterField::kRecordIn;
  }
  if (identifier == "record-out") {
    return TimelineTimecodeFilterField::kRecordOut;
  }
  return std::nullopt;
}

[[nodiscard]] std::string_view TrackKindIdentifier(
    core::TrackKind kind) noexcept {
  switch (kind) {
    case core::TrackKind::kVideo:
      return "video";
    case core::TrackKind::kAudio:
      return "audio";
    case core::TrackKind::kData:
      return "data";
    case core::TrackKind::kOther:
      return "other";
  }
  return {};
}

[[nodiscard]] std::optional<core::TrackKind> TrackKindFromIdentifier(
    std::string_view identifier) noexcept {
  if (identifier == "video") {
    return core::TrackKind::kVideo;
  }
  if (identifier == "audio") {
    return core::TrackKind::kAudio;
  }
  if (identifier == "data") {
    return core::TrackKind::kData;
  }
  if (identifier == "other") {
    return core::TrackKind::kOther;
  }
  return std::nullopt;
}

[[nodiscard]] std::string_view EditTypeIdentifier(
    core::EditType type) noexcept {
  switch (type) {
    case core::EditType::kCut:
      return "cut";
    case core::EditType::kDissolve:
      return "dissolve";
    case core::EditType::kWipe:
      return "wipe";
    case core::EditType::kKey:
      return "key";
    case core::EditType::kOther:
      return "other";
  }
  return {};
}

[[nodiscard]] std::optional<core::EditType> EditTypeFromIdentifier(
    std::string_view identifier) noexcept {
  if (identifier == "cut") {
    return core::EditType::kCut;
  }
  if (identifier == "dissolve") {
    return core::EditType::kDissolve;
  }
  if (identifier == "wipe") {
    return core::EditType::kWipe;
  }
  if (identifier == "key") {
    return core::EditType::kKey;
  }
  if (identifier == "other") {
    return core::EditType::kOther;
  }
  return std::nullopt;
}

[[nodiscard]] ValidationResult<Json> SerializeCondition(
    const TimelineFilterCondition& condition) {
  return std::visit(
      [](const auto& value) -> ValidationResult<Json> {
        using Condition = std::remove_cvref_t<decltype(value)>;
        if constexpr (std::same_as<Condition, TimelineTextFilterCondition>) {
          if (TextFieldIdentifier(value.field).empty()) {
            return std::unexpected(
                ValidationFailure{"Unknown text filter field."});
          }
          return Json{
              {"type", "text"},
              {"field", std::string{TextFieldIdentifier(value.field)}},
              {"text", value.text},
              {"match_case", value.match_case},
              {"match_whole_word", value.match_whole_word},
              {"regular_expression", value.regular_expression},
          };
        } else if constexpr (std::same_as<Condition,
                                          TimelineTrackKindFilterCondition>) {
          if (TrackKindIdentifier(value.track_kind).empty()) {
            return std::unexpected(ValidationFailure{"Unknown track kind."});
          }
          return Json{
              {"type", "track-kind"},
              {"value", std::string{TrackKindIdentifier(value.track_kind)}},
          };
        } else if constexpr (std::same_as<Condition,
                                          TimelineEditTypeFilterCondition>) {
          if (EditTypeIdentifier(value.edit_type).empty()) {
            return std::unexpected(ValidationFailure{"Unknown edit type."});
          }
          return Json{
              {"type", "edit-type"},
              {"value", std::string{EditTypeIdentifier(value.edit_type)}},
          };
        } else if constexpr (std::same_as<Condition,
                                          TimelineTimecodeFilterCondition>) {
          if (TimecodeFieldIdentifier(value.field).empty()) {
            return std::unexpected(
                ValidationFailure{"Unknown timecode filter field."});
          }
          return Json{
              {"type", "timecode"},
              {"field", std::string{TimecodeFieldIdentifier(value.field)}},
              {"value", value.timecode},
          };
        } else {
          return Json{
              {"type", "duration"},
              {"frames",
               value.frames.has_value() ? Json(*value.frames) : Json(nullptr)},
          };
        }
      },
      condition);
}

[[nodiscard]] ValidationResult<TimelineFilterCondition> DeserializeCondition(
    const Json& value) {
  if (!value.is_object() || !value.contains("type") ||
      !value.at("type").is_string()) {
    return std::unexpected(
        ValidationFailure{"Expected a condition type string."});
  }
  const auto type = value.at("type").get<std::string>();
  if (type == "text") {
    if (const auto shape = ValidateObject(
            value, {{"type", Json::value_t::string},
                    {"field", Json::value_t::string},
                    {"text", Json::value_t::string},
                    {"match_case", Json::value_t::boolean},
                    {"match_whole_word", Json::value_t::boolean},
                    {"regular_expression", Json::value_t::boolean}});
        !shape) {
      return std::unexpected(shape.error());
    }
    const auto field =
        TextFieldFromIdentifier(value.at("field").get<std::string>());
    if (!field.has_value()) {
      return std::unexpected(ValidationFailure{"Unknown text filter field."});
    }
    return TimelineTextFilterCondition{
        .field = *field,
        .text = value.at("text").get<std::string>(),
        .match_case = value.at("match_case").get<bool>(),
        .match_whole_word = value.at("match_whole_word").get<bool>(),
        .regular_expression = value.at("regular_expression").get<bool>(),
    };
  }
  if (type == "track-kind") {
    if (const auto shape =
            ValidateObject(value, {{"type", Json::value_t::string},
                                   {"value", Json::value_t::string}});
        !shape) {
      return std::unexpected(shape.error());
    }
    const auto track_kind =
        TrackKindFromIdentifier(value.at("value").get<std::string>());
    if (!track_kind.has_value()) {
      return std::unexpected(ValidationFailure{"Unknown track kind."});
    }
    return TimelineTrackKindFilterCondition{.track_kind = *track_kind};
  }
  if (type == "edit-type") {
    if (const auto shape =
            ValidateObject(value, {{"type", Json::value_t::string},
                                   {"value", Json::value_t::string}});
        !shape) {
      return std::unexpected(shape.error());
    }
    const auto edit_type =
        EditTypeFromIdentifier(value.at("value").get<std::string>());
    if (!edit_type.has_value()) {
      return std::unexpected(ValidationFailure{"Unknown edit type."});
    }
    return TimelineEditTypeFilterCondition{.edit_type = *edit_type};
  }
  if (type == "timecode") {
    if (const auto shape =
            ValidateObject(value, {{"type", Json::value_t::string},
                                   {"field", Json::value_t::string},
                                   {"value", Json::value_t::string}});
        !shape) {
      return std::unexpected(shape.error());
    }
    const auto field =
        TimecodeFieldFromIdentifier(value.at("field").get<std::string>());
    if (!field.has_value()) {
      return std::unexpected(
          ValidationFailure{"Unknown timecode filter field."});
    }
    return TimelineTimecodeFilterCondition{
        .field = *field,
        .timecode = value.at("value").get<std::string>(),
    };
  }
  if (type == "duration") {
    if (const auto shape = ValidateObject(
            value,
            {{"type", Json::value_t::string},
             {"frames", value.contains("frames") && value.at("frames").is_null()
                            ? Json::value_t::null
                            : Json::value_t::number_integer}});
        !shape) {
      return std::unexpected(shape.error());
    }
    const auto& frames = value.at("frames");
    if (frames.is_number_unsigned() &&
        frames.get<std::uint64_t>() >
            static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max())) {
      return std::unexpected(
          ValidationFailure{"Duration exceeds the signed 64-bit range."});
    }
    return TimelineDurationFilterCondition{
        .frames = frames.is_null()
                      ? std::nullopt
                      : std::optional<std::int64_t>{frames.get<std::int64_t>()},
    };
  }
  return std::unexpected(ValidationFailure{"Unknown filter condition type."});
}

[[nodiscard]] ValidationResult<Json> SerializeTemplate(
    const TimelineTemplate& value) {
  if (!IsValidIdentifier(value.identifier) || value.name.empty() ||
      !core::IsValidTimelineEventProjection(value.event_projection)) {
    return std::unexpected(ValidationFailure{"The template model is invalid."});
  }
  Json conditions = Json::array();
  for (const auto& condition : value.filter.conditions) {
    auto serialized = SerializeCondition(condition);
    if (!serialized) {
      return std::unexpected(serialized.error());
    }
    conditions.push_back(std::move(*serialized));
  }
  Json columns = Json::array();
  for (const auto field : value.event_projection) {
    columns.push_back(std::string{core::TimelineEventFieldIdentifier(field)});
  }
  std::string_view combination;
  switch (value.filter.combination) {
    case TimelineFilterCombination::kAll:
      combination = "all";
      break;
    case TimelineFilterCombination::kAny:
      combination = "any";
      break;
  }
  if (combination.empty()) {
    return std::unexpected(ValidationFailure{"Unknown filter combination."});
  }
  return Json{
      {"schema_version", kTimelineTemplateSchemaVersion},
      {"identifier", value.identifier},
      {"name", value.name},
      {"filter",
       {
           {"combination", std::string{combination}},
           {"conditions", std::move(conditions)},
       }},
      {"event_columns", std::move(columns)},
  };
}

[[nodiscard]] ValidationResult<TimelineTemplate> DeserializeTemplate(
    const Json& document) {
  if (const auto shape = ValidateObject(
          document, {{"schema_version", Json::value_t::number_integer},
                     {"identifier", Json::value_t::string},
                     {"name", Json::value_t::string},
                     {"filter", Json::value_t::object},
                     {"event_columns", Json::value_t::array}});
      !shape) {
    return std::unexpected(shape.error());
  }
  const auto& schema_version = document.at("schema_version");
  if (schema_version != kTimelineTemplateSchemaVersion) {
    return std::unexpected(ValidationFailure{
        "Unsupported template schema version " + schema_version.dump() + "."});
  }
  auto identifier = document.at("identifier").get<std::string>();
  auto name = document.at("name").get<std::string>();
  if (!IsValidIdentifier(identifier) || name.empty()) {
    return std::unexpected(
        ValidationFailure{"The template identity is invalid."});
  }

  const auto& filter = document.at("filter");
  if (const auto shape =
          ValidateObject(filter, {{"combination", Json::value_t::string},
                                  {"conditions", Json::value_t::array}});
      !shape) {
    return std::unexpected(shape.error());
  }
  const auto combination_text = filter.at("combination").get<std::string>();
  TimelineFilterCombination combination;
  if (combination_text == "all") {
    combination = TimelineFilterCombination::kAll;
  } else if (combination_text == "any") {
    combination = TimelineFilterCombination::kAny;
  } else {
    return std::unexpected(ValidationFailure{"Unknown filter combination."});
  }
  std::vector<TimelineFilterCondition> conditions;
  for (const auto& condition : filter.at("conditions")) {
    auto deserialized = DeserializeCondition(condition);
    if (!deserialized) {
      return std::unexpected(deserialized.error());
    }
    conditions.push_back(std::move(*deserialized));
  }

  std::vector<core::TimelineEventField> projection;
  for (const auto& column : document.at("event_columns")) {
    if (!column.is_string()) {
      return std::unexpected(
          ValidationFailure{"Expected an event column string."});
    }
    const auto field =
        core::TimelineEventFieldFromIdentifier(column.get<std::string>());
    if (!field.has_value()) {
      return std::unexpected(ValidationFailure{"Unknown event column."});
    }
    projection.push_back(*field);
  }
  if (!core::IsValidTimelineEventProjection(projection)) {
    return std::unexpected(
        ValidationFailure{"The event column projection is invalid."});
  }

  return TimelineTemplate{
      .identifier = std::move(identifier),
      .name = std::move(name),
      .filter =
          TimelineFilterQuery{
              .combination = combination,
              .conditions = std::move(conditions),
          },
      .event_projection = std::move(projection),
  };
}

[[nodiscard]] TimelineTemplateStoreFailure Failure(
    TimelineTemplateStoreFailureKind kind, const std::filesystem::path& path,
    std::error_code error, std::string message) {
  return TimelineTemplateStoreFailure{
      .kind = kind,
      .path = path,
      .filesystem_error = error,
      .message = std::move(message),
  };
}

[[nodiscard]] TimelineTemplateStoreFailure WriteFailure(
    const storage::LocalFileFailure& failure) {
  auto kind = TimelineTemplateStoreFailureKind::kWriteFailed;
  if (failure.kind == storage::LocalFileFailureKind::kCommitFailed) {
    kind = TimelineTemplateStoreFailureKind::kCommitFailed;
  }
  return Failure(kind, failure.path, failure.filesystem_error,
                 failure.filesystem_error.message());
}

/// Reads, parses, and validates one template file.
[[nodiscard]] std::expected<TimelineTemplate,
                            TimelineTemplateStoreLoadDiagnostic>
LoadTemplateFile(const std::filesystem::path& path) {
  try {
    const auto content = storage::ReadLocalFile(path);
    if (!content.has_value()) {
      return std::unexpected(TimelineTemplateStoreLoadDiagnostic{
          .path = path,
          .message = content.error().filesystem_error.message(),
      });
    }
    const std::string serialized =
        content->empty()
            ? std::string{}
            : std::string{reinterpret_cast<const char*>(content->data()),
                          content->size()};
    const auto document = Json::parse(serialized, nullptr, false);
    if (document.is_discarded()) {
      return std::unexpected(TimelineTemplateStoreLoadDiagnostic{
          .path = path,
          .message = "Malformed template JSON.",
      });
    }
    auto value = DeserializeTemplate(document);
    if (!value) {
      return std::unexpected(TimelineTemplateStoreLoadDiagnostic{
          .path = path,
          .message = std::move(value.error().message),
      });
    }
    if (path.stem() != value->identifier) {
      return std::unexpected(TimelineTemplateStoreLoadDiagnostic{
          .path = path,
          .message = "The filename does not match the template identifier.",
      });
    }
    return std::move(*value);
  } catch (const std::exception& exception) {
    return std::unexpected(TimelineTemplateStoreLoadDiagnostic{
        .path = path,
        .message = exception.what(),
    });
  }
}

}  // namespace

TimelineTemplateStore::TimelineTemplateStore(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

TimelineTemplateLoadOutcome TimelineTemplateStore::Load(void) const {
  std::error_code error;
  if (!std::filesystem::exists(directory_, error)) {
    if (error) {
      return std::unexpected(
          Failure(TimelineTemplateStoreFailureKind::kStorageUnavailable,
                  directory_, error, "Could not inspect the template store."));
    }
    return TimelineTemplateStoreLoadResult{};
  }

  TimelineTemplateStoreLoadResult result;
  std::filesystem::directory_iterator iterator{directory_, error};
  if (error) {
    return std::unexpected(
        Failure(TimelineTemplateStoreFailureKind::kStorageUnavailable,
                directory_, error, "Could not enumerate the template store."));
  }
  try {
    for (const auto& entry : iterator) {
      if (!entry.is_regular_file(error)) {
        if (error) {
          return std::unexpected(
              Failure(TimelineTemplateStoreFailureKind::kStorageUnavailable,
                      entry.path(), error,
                      "Could not inspect a template store entry."));
        }
        continue;
      }
      if (entry.path().extension().string() != kFileExtension) {
        continue;
      }
      auto loaded = LoadTemplateFile(entry.path());
      if (loaded.has_value()) {
        result.templates.push_back(std::move(*loaded));
      } else {
        result.diagnostics.push_back(std::move(loaded.error()));
      }
    }
  } catch (const std::filesystem::filesystem_error& exception) {
    return std::unexpected(Failure(
        TimelineTemplateStoreFailureKind::kStorageUnavailable, directory_,
        exception.code(), "Could not enumerate the template store."));
  }
  std::ranges::sort(result.templates, {}, &TimelineTemplate::name);
  return result;
}

TimelineTemplateMutationOutcome TimelineTemplateStore::Save(
    const TimelineTemplate& value) const {
  std::string content;
  try {
    auto serialized = SerializeTemplate(value);
    if (!serialized) {
      return std::unexpected(
          Failure(TimelineTemplateStoreFailureKind::kSerializationFailed,
                  directory_, {}, std::move(serialized.error().message)));
    }
    content = serialized->dump(2);
    content.push_back('\n');
  } catch (const std::exception& exception) {
    return std::unexpected(
        Failure(TimelineTemplateStoreFailureKind::kSerializationFailed,
                directory_, {}, exception.what()));
  }

  std::error_code error;
  std::filesystem::create_directories(directory_, error);
  if (error) {
    return std::unexpected(
        Failure(TimelineTemplateStoreFailureKind::kStorageUnavailable,
                directory_, error, "Could not create the template store."));
  }
  const auto destination = directory_ / (value.identifier + ".json");
  const auto bytes = std::as_bytes(std::span{content});
  const auto result = storage::WriteLocalFileAtomically(
      destination, bytes, storage::ExistingFilePolicy::kReplace);
  if (!result.has_value()) {
    return std::unexpected(WriteFailure(result.error()));
  }
  return {};
}

TimelineTemplateMutationOutcome TimelineTemplateStore::Remove(
    std::string_view identifier) const {
  if (!IsValidIdentifier(identifier)) {
    return std::unexpected(
        Failure(TimelineTemplateStoreFailureKind::kRemoveFailed, directory_,
                std::make_error_code(std::errc::invalid_argument),
                "The template identifier is invalid."));
  }
  const auto path = directory_ / (std::string{identifier} + ".json");
  std::error_code error;
  static_cast<void>(std::filesystem::remove(path, error));
  if (error) {
    return std::unexpected(
        Failure(TimelineTemplateStoreFailureKind::kRemoveFailed, path, error,
                "Could not remove the template file."));
  }
  return {};
}

}  // namespace edit_atlas::services
