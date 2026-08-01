#include <edit_atlas/services/timeline_filter.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <re2/re2.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace edit_atlas::services {
namespace {

struct PreparedTextCondition final {
    TimelineTextFilterField field;
    std::unique_ptr<RE2> expression;
};

using PreparedCondition =
    std::variant<PreparedTextCondition, TimelineTrackKindFilterCondition,
                 TimelineEditTypeFilterCondition,
                 TimelineTimecodeFilterCondition,
                 TimelineDurationFilterCondition>;
using PreparedConditions = std::vector<PreparedCondition>;

[[nodiscard]] std::string
ExpressionText(const TimelineTextFilterCondition &condition) {
    auto expression = condition.regular_expression
                          ? condition.text
                          : RE2::QuoteMeta(condition.text);
    if (condition.match_whole_word) {
        expression = R"((?:^|[^\p{L}\p{N}_])(?:)" + expression +
                     R"()(?:$|[^\p{L}\p{N}_]))";
    }
    return expression;
}

[[nodiscard]] std::expected<PreparedConditions, TimelineFilterError>
PrepareConditions(const TimelineFilterQuery &query) {
    PreparedConditions prepared;
    prepared.reserve(query.conditions.size());
    for (std::size_t index = 0; index < query.conditions.size(); ++index) {
        const auto &condition = query.conditions[index];
        if (const auto *track_kind =
                std::get_if<TimelineTrackKindFilterCondition>(&condition)) {
            prepared.emplace_back(*track_kind);
            continue;
        }
        if (const auto *edit_type =
                std::get_if<TimelineEditTypeFilterCondition>(&condition)) {
            prepared.emplace_back(*edit_type);
            continue;
        }
        if (const auto *timecode =
                std::get_if<TimelineTimecodeFilterCondition>(&condition)) {
            if (timecode->timecode.empty()) {
                continue;
            }
            prepared.emplace_back(*timecode);
            continue;
        }
        if (const auto *duration =
                std::get_if<TimelineDurationFilterCondition>(&condition)) {
            if (!duration->frames.has_value()) {
                continue;
            }
            prepared.emplace_back(*duration);
            continue;
        }
        const auto &text_condition =
            std::get<TimelineTextFilterCondition>(condition);
        if (text_condition.text.empty()) {
            continue;
        }
        RE2::Options options;
        options.set_case_sensitive(text_condition.match_case);
        options.set_encoding(RE2::Options::EncodingUTF8);
        options.set_log_errors(false);
        auto expression =
            std::make_unique<RE2>(ExpressionText(text_condition), options);
        if (!expression->ok()) {
            return std::unexpected(TimelineFilterError{
                .condition_index = index,
                .message = expression->error(),
            });
        }
        prepared.emplace_back(PreparedTextCondition{
            .field = text_condition.field,
            .expression = std::move(expression),
        });
    }
    return prepared;
}

[[nodiscard]] bool MatchesText(std::string_view value, const RE2 &expression) {
    return RE2::PartialMatch(re2::StringPiece{value.data(), value.size()},
                             expression);
}

[[nodiscard]] std::string TwoDigits(std::int32_t value) {
    auto text = std::to_string(value);
    if (text.size() < 2) {
        text.insert(text.begin(), '0');
    }
    return text;
}

[[nodiscard]] std::string TimecodeText(const core::Timecode &timecode) {
    const auto separator =
        timecode.mode() == core::TimecodeMode::kDropFrame ? ';' : ':';
    return TwoDigits(timecode.hours()) + ':' + TwoDigits(timecode.minutes()) +
           ':' + TwoDigits(timecode.seconds()) + separator +
           TwoDigits(timecode.frames());
}

[[nodiscard]] std::string_view MetadataText(const core::EditEvent &event,
                                            std::string_view key) {
    const auto match =
        std::ranges::find(event.metadata, key, &core::MetadataEntry::key);
    if (match == event.metadata.end()) {
        return {};
    }
    const auto *text = std::get_if<std::string>(&match->value);
    return text == nullptr ? std::string_view{} : std::string_view{*text};
}

[[nodiscard]] bool CommentsMatch(const core::EditEvent &event,
                                 const RE2 &expression) {
    return std::ranges::any_of(event.comments,
                               [&](const core::Comment &comment) {
                                   return MatchesText(comment.text, expression);
                               });
}

[[nodiscard]] bool TextFieldMatches(const core::EditEvent &event,
                                    TimelineTextFilterField field,
                                    const RE2 &expression) {
    switch (field) {
    case TimelineTextFilterField::kEventIdentifier:
        return MatchesText(event.identifier, expression);
    case TimelineTextFilterField::kReel:
        return MatchesText(event.reel, expression);
    case TimelineTextFilterField::kTrackIdentifier:
        return MatchesText(event.track.identifier, expression);
    case TimelineTextFilterField::kClip:
        return MatchesText(MetadataText(event, "clip_name"), expression);
    case TimelineTextFilterField::kComments:
        return CommentsMatch(event, expression);
    }
    return false;
}

[[nodiscard]] bool ConditionMatches(const core::EditEvent &event,
                                    const PreparedTextCondition &condition) {
    return TextFieldMatches(event, condition.field, *condition.expression);
}

[[nodiscard]] bool
ConditionMatches(const core::EditEvent &event,
                 const TimelineTrackKindFilterCondition &condition) {
    return event.track.kind == condition.track_kind;
}

[[nodiscard]] bool
ConditionMatches(const core::EditEvent &event,
                 const TimelineEditTypeFilterCondition &condition) {
    return event.edit_type == condition.edit_type;
}

[[nodiscard]] bool
ConditionMatches(const core::EditEvent &event,
                 const TimelineTimecodeFilterCondition &condition) {
    switch (condition.field) {
    case TimelineTimecodeFilterField::kSourceIn:
        return TimecodeText(event.source_range.start()) == condition.timecode;
    case TimelineTimecodeFilterField::kSourceOut:
        return TimecodeText(event.source_range.end_exclusive()) ==
               condition.timecode;
    case TimelineTimecodeFilterField::kRecordIn:
        return TimecodeText(event.record_range.start()) == condition.timecode;
    case TimelineTimecodeFilterField::kRecordOut:
        return TimecodeText(event.record_range.end_exclusive()) ==
               condition.timecode;
    }
    return false;
}

[[nodiscard]] bool
ConditionMatches(const core::EditEvent &event,
                 const TimelineDurationFilterCondition &condition) {
    return condition.frames.has_value() &&
           event.record_range.DurationInFrames() == *condition.frames;
}

[[nodiscard]] bool EventMatches(const core::EditEvent &event,
                                TimelineFilterCombination combination,
                                const PreparedConditions &conditions) {
    bool matched_any = false;
    for (const auto &condition : conditions) {
        const auto matches = std::visit(
            [&event](const auto &value) {
                return ConditionMatches(event, value);
            },
            condition);
        if (combination == TimelineFilterCombination::kAll && !matches) {
            return false;
        }
        matched_any = matched_any || matches;
    }
    return conditions.empty() ||
           combination == TimelineFilterCombination::kAll || matched_any;
}

} // namespace

TimelineFilterResult
FilterTimelineEvents(const core::TimelineDocument &document,
                     const TimelineFilterQuery &query) {
    auto prepared = PrepareConditions(query);
    if (!prepared.has_value()) {
        return std::unexpected(std::move(prepared.error()));
    }
    TimelineEventSelection selection;
    selection.reserve(document.events.size());
    for (std::size_t index = 0; index < document.events.size(); ++index) {
        if (EventMatches(document.events[index], query.combination,
                         *prepared)) {
            selection.emplace_back(index);
        }
    }
    return selection;
}

core::TimelineDocument
SelectTimelineEvents(const core::TimelineDocument &document,
                     std::span<const std::size_t> selection) {
    core::TimelineDocument selected_document{
        .title = document.title,
        .frame_rate = document.frame_rate,
        .timecode_mode = document.timecode_mode,
        .events = {},
        .metadata = document.metadata,
        .diagnostics = document.diagnostics,
        .provenance = document.provenance,
    };
    selected_document.events.reserve(selection.size());
    for (const auto index : selection) {
        if (index < document.events.size()) {
            selected_document.events.emplace_back(document.events[index]);
        }
    }
    return selected_document;
}

} // namespace edit_atlas::services
