#include "timeline_template_store.hpp"

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_filter.hpp>

#include <edit_atlas/storage/local_file.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace edit_atlas::services {
namespace {

using Json = nlohmann::json;

constexpr std::string_view kFileExtension = ".json";

[[nodiscard]] bool IsValidIdentifier(std::string_view identifier) {
    return !identifier.empty() &&
           std::ranges::all_of(identifier, [](char character) {
               return (character >= 'a' && character <= 'z') ||
                      (character >= '0' && character <= '9') ||
                      character == '-';
           });
}

void RejectUnknownKeys(const Json &value,
                       std::initializer_list<std::string_view> supported_keys) {
    if (!value.is_object()) {
        throw std::runtime_error{"Expected a JSON object."};
    }
    for (const auto &item : value.items()) {
        if (!std::ranges::contains(supported_keys, item.key())) {
            throw std::runtime_error{"Unknown field \"" + item.key() + "\"."};
        }
    }
}

[[nodiscard]] std::string RequiredIdentifier(std::string_view identifier,
                                             std::string_view description) {
    if (identifier.empty()) {
        throw std::runtime_error{"Unknown " + std::string{description} + "."};
    }
    return std::string{identifier};
}

[[nodiscard]] std::string_view
TextFieldIdentifier(TimelineTextFilterField field) noexcept {
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

[[nodiscard]] std::optional<TimelineTextFilterField>
TextFieldFromIdentifier(std::string_view identifier) noexcept {
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

[[nodiscard]] std::string_view
TimecodeFieldIdentifier(TimelineTimecodeFilterField field) noexcept {
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

[[nodiscard]] std::string_view
TrackKindIdentifier(core::TrackKind kind) noexcept {
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

[[nodiscard]] std::optional<core::TrackKind>
TrackKindFromIdentifier(std::string_view identifier) noexcept {
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

[[nodiscard]] std::string_view
EditTypeIdentifier(core::EditType type) noexcept {
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

[[nodiscard]] std::optional<core::EditType>
EditTypeFromIdentifier(std::string_view identifier) noexcept {
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

[[nodiscard]] Json
SerializeCondition(const TimelineFilterCondition &condition) {
    return std::visit(
        [](const auto &value) -> Json {
            using Condition = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<Condition,
                                         TimelineTextFilterCondition>) {
                return Json{
                    {"type", "text"},
                    {"field",
                     RequiredIdentifier(TextFieldIdentifier(value.field),
                                        "text filter field")},
                    {"text", value.text},
                    {"match_case", value.match_case},
                    {"match_whole_word", value.match_whole_word},
                    {"regular_expression", value.regular_expression},
                };
            } else if constexpr (std::is_same_v<
                                     Condition,
                                     TimelineTrackKindFilterCondition>) {
                return Json{
                    {"type", "track-kind"},
                    {"value",
                     RequiredIdentifier(TrackKindIdentifier(value.track_kind),
                                        "track kind")},
                };
            } else if constexpr (std::is_same_v<
                                     Condition,
                                     TimelineEditTypeFilterCondition>) {
                return Json{
                    {"type", "edit-type"},
                    {"value",
                     RequiredIdentifier(EditTypeIdentifier(value.edit_type),
                                        "edit type")},
                };
            } else if constexpr (std::is_same_v<
                                     Condition,
                                     TimelineTimecodeFilterCondition>) {
                return Json{
                    {"type", "timecode"},
                    {"field",
                     RequiredIdentifier(TimecodeFieldIdentifier(value.field),
                                        "timecode filter field")},
                    {"value", value.timecode},
                };
            } else {
                return Json{
                    {"type", "duration"},
                    {"frames", value.frames.has_value() ? Json(*value.frames)
                                                        : Json(nullptr)},
                };
            }
        },
        condition);
}

[[nodiscard]] TimelineFilterCondition DeserializeCondition(const Json &value) {
    RejectUnknownKeys(value, {"type", "field", "text", "match_case",
                              "match_whole_word", "regular_expression", "value",
                              "frames"});
    const auto type = value.at("type").get<std::string>();
    if (type == "text") {
        RejectUnknownKeys(value, {"type", "field", "text", "match_case",
                                  "match_whole_word", "regular_expression"});
        const auto field =
            TextFieldFromIdentifier(value.at("field").get<std::string>());
        if (!field.has_value()) {
            throw std::runtime_error{"Unknown text filter field."};
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
        RejectUnknownKeys(value, {"type", "value"});
        const auto track_kind =
            TrackKindFromIdentifier(value.at("value").get<std::string>());
        if (!track_kind.has_value()) {
            throw std::runtime_error{"Unknown track kind."};
        }
        return TimelineTrackKindFilterCondition{.track_kind = *track_kind};
    }
    if (type == "edit-type") {
        RejectUnknownKeys(value, {"type", "value"});
        const auto edit_type =
            EditTypeFromIdentifier(value.at("value").get<std::string>());
        if (!edit_type.has_value()) {
            throw std::runtime_error{"Unknown edit type."};
        }
        return TimelineEditTypeFilterCondition{.edit_type = *edit_type};
    }
    if (type == "timecode") {
        RejectUnknownKeys(value, {"type", "field", "value"});
        const auto field =
            TimecodeFieldFromIdentifier(value.at("field").get<std::string>());
        if (!field.has_value()) {
            throw std::runtime_error{"Unknown timecode filter field."};
        }
        return TimelineTimecodeFilterCondition{
            .field = *field,
            .timecode = value.at("value").get<std::string>(),
        };
    }
    if (type == "duration") {
        RejectUnknownKeys(value, {"type", "frames"});
        const auto &frames = value.at("frames");
        return TimelineDurationFilterCondition{
            .frames =
                frames.is_null()
                    ? std::nullopt
                    : std::optional<std::int64_t>{frames.get<std::int64_t>()},
        };
    }
    throw std::runtime_error{"Unknown filter condition type."};
}

[[nodiscard]] Json SerializeTemplate(const TimelineTemplate &value) {
    if (!IsValidIdentifier(value.identifier) || value.name.empty() ||
        !core::IsValidTimelineEventProjection(value.event_projection)) {
        throw std::runtime_error{"The template model is invalid."};
    }
    Json conditions = Json::array();
    for (const auto &condition : value.filter.conditions) {
        conditions.push_back(SerializeCondition(condition));
    }
    Json columns = Json::array();
    for (const auto field : value.event_projection) {
        columns.push_back(
            std::string{core::TimelineEventFieldIdentifier(field)});
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
        throw std::runtime_error{"Unknown filter combination."};
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

[[nodiscard]] TimelineTemplate DeserializeTemplate(const Json &document) {
    RejectUnknownKeys(document, {"schema_version", "identifier", "name",
                                 "filter", "event_columns"});
    const auto schema_version = document.at("schema_version").get<int>();
    if (schema_version != kTimelineTemplateSchemaVersion) {
        throw std::runtime_error{"Unsupported template schema version " +
                                 std::to_string(schema_version) + "."};
    }
    auto identifier = document.at("identifier").get<std::string>();
    auto name = document.at("name").get<std::string>();
    if (!IsValidIdentifier(identifier) || name.empty()) {
        throw std::runtime_error{"The template identity is invalid."};
    }

    const auto &filter = document.at("filter");
    RejectUnknownKeys(filter, {"combination", "conditions"});
    const auto combination_text = filter.at("combination").get<std::string>();
    TimelineFilterCombination combination;
    if (combination_text == "all") {
        combination = TimelineFilterCombination::kAll;
    } else if (combination_text == "any") {
        combination = TimelineFilterCombination::kAny;
    } else {
        throw std::runtime_error{"Unknown filter combination."};
    }
    std::vector<TimelineFilterCondition> conditions;
    for (const auto &condition : filter.at("conditions")) {
        conditions.push_back(DeserializeCondition(condition));
    }

    std::vector<core::TimelineEventField> projection;
    for (const auto &column : document.at("event_columns")) {
        const auto field =
            core::TimelineEventFieldFromIdentifier(column.get<std::string>());
        if (!field.has_value()) {
            throw std::runtime_error{"Unknown event column."};
        }
        projection.push_back(*field);
    }
    if (!core::IsValidTimelineEventProjection(projection)) {
        throw std::runtime_error{"The event column projection is invalid."};
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

[[nodiscard]] TimelineTemplateStoreFailure
Failure(TimelineTemplateStoreFailureKind kind,
        const std::filesystem::path &path, std::error_code error,
        std::string message) {
    return TimelineTemplateStoreFailure{
        .kind = kind,
        .path = path,
        .filesystem_error = error,
        .message = std::move(message),
    };
}

[[nodiscard]] TimelineTemplateStoreFailure
WriteFailure(const storage::LocalFileFailure &failure) {
    auto kind = TimelineTemplateStoreFailureKind::kWriteFailed;
    if (failure.kind == storage::LocalFileFailureKind::kCommitFailed) {
        kind = TimelineTemplateStoreFailureKind::kCommitFailed;
    }
    return Failure(kind, failure.path, failure.filesystem_error,
                   failure.filesystem_error.message());
}

} // namespace

TimelineTemplateStore::TimelineTemplateStore(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

TimelineTemplateLoadOutcome TimelineTemplateStore::Load(void) const {
    std::error_code error;
    if (!std::filesystem::exists(directory_, error)) {
        if (error) {
            return std::unexpected(Failure(
                TimelineTemplateStoreFailureKind::kStorageUnavailable,
                directory_, error, "Could not inspect the template store."));
        }
        return TimelineTemplateStoreLoadResult{};
    }

    TimelineTemplateStoreLoadResult result;
    std::filesystem::directory_iterator iterator{directory_, error};
    if (error) {
        return std::unexpected(Failure(
            TimelineTemplateStoreFailureKind::kStorageUnavailable, directory_,
            error, "Could not enumerate the template store."));
    }
    try {
        for (const auto &entry : iterator) {
            if (!entry.is_regular_file(error)) {
                if (error) {
                    return std::unexpected(Failure(
                        TimelineTemplateStoreFailureKind::kStorageUnavailable,
                        entry.path(), error,
                        "Could not inspect a template store entry."));
                }
                continue;
            }
            if (entry.path().extension().string() != kFileExtension) {
                continue;
            }
            try {
                const auto content = storage::ReadLocalFile(entry.path());
                if (!content.has_value()) {
                    result.diagnostics.push_back({
                        .path = entry.path(),
                        .message = content.error().filesystem_error.message(),
                    });
                    continue;
                }
                const std::string serialized =
                    content->empty()
                        ? std::string{}
                        : std::string{
                              reinterpret_cast<const char *>(content->data()),
                              content->size()};
                auto value = DeserializeTemplate(Json::parse(serialized));
                if (entry.path().stem() != value.identifier) {
                    throw std::runtime_error{
                        "The filename does not match the template identifier."};
                }
                result.templates.push_back(std::move(value));
            } catch (const std::exception &exception) {
                result.diagnostics.push_back({
                    .path = entry.path(),
                    .message = exception.what(),
                });
            }
        }
    } catch (const std::filesystem::filesystem_error &exception) {
        return std::unexpected(Failure(
            TimelineTemplateStoreFailureKind::kStorageUnavailable, directory_,
            exception.code(), "Could not enumerate the template store."));
    }
    std::ranges::sort(result.templates, {}, &TimelineTemplate::name);
    return result;
}

TimelineTemplateMutationOutcome
TimelineTemplateStore::Save(const TimelineTemplate &value) const {
    std::string content;
    try {
        content = SerializeTemplate(value).dump(2);
        content.push_back('\n');
    } catch (const std::exception &exception) {
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

TimelineTemplateMutationOutcome
TimelineTemplateStore::Remove(std::string_view identifier) const {
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
            Failure(TimelineTemplateStoreFailureKind::kRemoveFailed, path,
                    error, "Could not remove the template file."));
    }
    return {};
}

} // namespace edit_atlas::services
