#include <edit_atlas/core/timeline_projection.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <utility>

namespace edit_atlas::core {
namespace {

using FieldIdentifier = std::pair<TimelineEventField, std::string_view>;

constexpr std::array kFieldIdentifiers{
    FieldIdentifier{TimelineEventField::kEventIdentifier, "event"},
    FieldIdentifier{TimelineEventField::kReel, "reel"},
    FieldIdentifier{TimelineEventField::kTrackKind, "track-kind"},
    FieldIdentifier{TimelineEventField::kTrackIdentifier, "track"},
    FieldIdentifier{TimelineEventField::kEditType, "edit-type"},
    FieldIdentifier{TimelineEventField::kTransitionIdentifier, "transition"},
    FieldIdentifier{TimelineEventField::kTransitionDuration,
                    "transition-frames"},
    FieldIdentifier{TimelineEventField::kSourceIn, "source-in"},
    FieldIdentifier{TimelineEventField::kSourceOut, "source-out"},
    FieldIdentifier{TimelineEventField::kRecordIn, "record-in"},
    FieldIdentifier{TimelineEventField::kRecordOut, "record-out"},
    FieldIdentifier{TimelineEventField::kDuration, "duration"},
    FieldIdentifier{TimelineEventField::kDurationFrames, "duration-frames"},
    FieldIdentifier{TimelineEventField::kClipName, "clip-name"},
    FieldIdentifier{TimelineEventField::kSourceFile, "source-file"},
    FieldIdentifier{TimelineEventField::kComments, "comments"},
    FieldIdentifier{TimelineEventField::kSourceLine, "source-line"},
};

static_assert(kFieldIdentifiers.size() == kTimelineEventFieldCount);

constexpr auto kDefaultProjection = [] {
    std::array<TimelineEventField, kFieldIdentifiers.size()> projection{};
    for (std::size_t index = 0; index < kFieldIdentifiers.size(); ++index) {
        projection[index] = kFieldIdentifiers[index].first;
    }
    return projection;
}();

} // namespace

std::string_view
TimelineEventFieldIdentifier(TimelineEventField field) noexcept {
    const auto item =
        std::ranges::find(kFieldIdentifiers, field, &FieldIdentifier::first);
    return item == kFieldIdentifiers.end() ? std::string_view{} : item->second;
}

std::optional<TimelineEventField>
TimelineEventFieldFromIdentifier(std::string_view identifier) noexcept {
    const auto item = std::ranges::find(kFieldIdentifiers, identifier,
                                        &FieldIdentifier::second);
    if (item == kFieldIdentifiers.end()) {
        return std::nullopt;
    }
    return item->first;
}

std::span<const TimelineEventField>
DefaultTimelineEventProjection(void) noexcept {
    return kDefaultProjection;
}

bool IsValidTimelineEventProjection(
    std::span<const TimelineEventField> projection) noexcept {
    for (auto item = projection.begin(); item != projection.end(); ++item) {
        if (TimelineEventFieldIdentifier(*item).empty() ||
            std::find(std::next(item), projection.end(), *item) !=
                projection.end()) {
            return false;
        }
    }
    return !projection.empty();
}

} // namespace edit_atlas::core
