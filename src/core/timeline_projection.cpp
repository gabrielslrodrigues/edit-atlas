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

#include "edit_atlas/core/timeline_projection.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <string_view>

namespace edit_atlas::core {
namespace {

struct FieldIdentifier final {
  TimelineEventField field;
  std::string_view identifier;
};

constexpr std::array kFieldIdentifiers{
    FieldIdentifier{TimelineEventField::kEventIdentifier, "event"},
    FieldIdentifier{TimelineEventField::kInitialFrame, "initial-frame"},
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

constexpr auto kAllFields = [] {
  std::array<TimelineEventField, kFieldIdentifiers.size()> fields{};
  for (std::size_t index = 0; index < kFieldIdentifiers.size(); ++index) {
    fields[index] = kFieldIdentifiers[index].field;
  }
  return fields;
}();

constexpr auto kDefaultProjection = [] {
  std::array<TimelineEventField, kFieldIdentifiers.size() - 1> projection{};
  std::size_t output_index = 0;
  for (const auto field : kAllFields) {
    if (field != TimelineEventField::kInitialFrame) {
      projection[output_index++] = field;
    }
  }
  return projection;
}();

}  // namespace

std::string_view TimelineEventFieldIdentifier(
    TimelineEventField field) noexcept {
  const auto item =
      std::ranges::find(kFieldIdentifiers, field, &FieldIdentifier::field);
  return item == kFieldIdentifiers.end() ? std::string_view{}
                                         : item->identifier;
}

std::optional<TimelineEventField> TimelineEventFieldFromIdentifier(
    std::string_view identifier) noexcept {
  const auto item = std::ranges::find(kFieldIdentifiers, identifier,
                                      &FieldIdentifier::identifier);
  if (item == kFieldIdentifiers.end()) {
    return std::nullopt;
  }
  return item->field;
}

std::span<const TimelineEventField> TimelineEventFields(void) noexcept {
  return kAllFields;
}

std::span<const TimelineEventField> DefaultTimelineEventProjection(
    void) noexcept {
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

}  // namespace edit_atlas::core
