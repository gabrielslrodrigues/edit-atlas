#ifndef EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_HPP_

#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_filter.hpp>

#include <string>
#include <vector>

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
    bool operator==(const TimelineTemplate &) const = default;
};

/// Generates a random lowercase identifier suitable for a new template.
[[nodiscard]] std::string GenerateTimelineTemplateIdentifier(void);

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_HPP_
