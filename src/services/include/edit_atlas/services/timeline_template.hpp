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

#ifndef EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_HPP_

#include <string>
#include <vector>

#include "edit_atlas/core/timeline_projection.hpp"
#include "edit_atlas/services/timeline_filter.hpp"

namespace edit_atlas::services {

/// A named reusable combination of filtering and export choices.
struct TimelineTemplate final {
  /// Stable, non-localized identity used by persistence and frontends.
  std::string identifier;
  /// User-provided display name.
  std::string name;
  /// Presentation-independent event filtering configuration.
  TimelineFilterQuery filter;
  /// Ordered event fields included by exports using this template.
  std::vector<core::TimelineEventField> event_projection;

  /// Compares the identifier, name, filter, and event projection.
  bool operator==(const TimelineTemplate&) const = default;
};

/// Generates a random lowercase identifier suitable for a new template.
[[nodiscard]] std::string GenerateTimelineTemplateIdentifier(void);

}  // namespace edit_atlas::services

#endif  // EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_HPP_
