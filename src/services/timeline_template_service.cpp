#include <edit_atlas/services/timeline_template_service.hpp>

#include "timeline_template_store.hpp"

#include <edit_atlas/core/timeline_projection.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::services {
namespace {

[[nodiscard]] TimelineTemplateFailure
ServiceFailure(TimelineTemplateFailureKind kind, std::string message) {
    return TimelineTemplateFailure{
        .kind = kind,
        .path = {},
        .filesystem_error = {},
        .message = std::move(message),
    };
}

[[nodiscard]] TimelineTemplateFailure
ServiceFailure(const TimelineTemplateStoreFailure &failure) {
    return TimelineTemplateFailure{
        .kind = TimelineTemplateFailureKind::kStorageFailed,
        .path = failure.path,
        .filesystem_error = failure.filesystem_error,
        .message = failure.message,
    };
}

} // namespace

TimelineTemplateService::TimelineTemplateService(
    std::filesystem::path directory)
    : directory_(std::move(directory)) {}

TimelineTemplateLoadResult TimelineTemplateService::Load(void) {
    const TimelineTemplateStore store{directory_};
    auto result = store.Load();
    if (!result.has_value()) {
        return std::unexpected(ServiceFailure(result.error()));
    }
    std::vector<TimelineTemplateLoadDiagnostic> diagnostics;
    diagnostics.reserve(result->diagnostics.size());
    for (auto &diagnostic : result->diagnostics) {
        diagnostics.push_back({
            .path = std::move(diagnostic.path),
            .message = std::move(diagnostic.message),
        });
    }
    templates_.clear();
    templates_.reserve(result->templates.size());
    for (auto &value : result->templates) {
        const auto duplicate_name =
            std::ranges::any_of(templates_, [&value](const auto &existing) {
                return existing.name == value.name;
            });
        if (duplicate_name) {
            diagnostics.push_back({
                .path = directory_ / (value.identifier + ".json"),
                .message = "The template name is already in use.",
            });
            continue;
        }
        templates_.push_back(std::move(value));
    }
    loaded_ = true;
    return diagnostics;
}

std::span<const TimelineTemplate>
TimelineTemplateService::Templates(void) const {
    return templates_;
}

const TimelineTemplate *
TimelineTemplateService::Find(std::string_view identifier) const {
    const auto position = std::ranges::find(templates_, identifier,
                                            &TimelineTemplate::identifier);
    return position == templates_.end() ? nullptr : &*position;
}

TimelineTemplateResult TimelineTemplateService::Create(
    std::string name, TimelineFilterQuery filter,
    std::vector<core::TimelineEventField> event_projection) {
    const auto validation = ValidateName(name);
    if (!validation.has_value()) {
        return std::unexpected(validation.error());
    }
    if (!core::IsValidTimelineEventProjection(event_projection)) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kInvalidTemplate,
                           "The event column projection is invalid."));
    }

    TimelineTemplate value{
        .identifier = GenerateTimelineTemplateIdentifier(),
        .name = std::move(name),
        .filter = std::move(filter),
        .event_projection = std::move(event_projection),
    };
    while (Find(value.identifier) != nullptr) {
        value.identifier = GenerateTimelineTemplateIdentifier();
    }
    const TimelineTemplateStore store{directory_};
    const auto result = store.Save(value);
    if (!result.has_value()) {
        return std::unexpected(ServiceFailure(result.error()));
    }
    templates_.push_back(value);
    Sort();
    return value;
}

TimelineTemplateResult TimelineTemplateService::Update(
    std::string_view identifier, TimelineFilterQuery filter,
    std::vector<core::TimelineEventField> event_projection) {
    if (!loaded_) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotLoaded,
                           "The template catalog has not been loaded."));
    }
    auto *existing = FindMutable(identifier);
    if (existing == nullptr) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotFound,
                           "The template does not exist."));
    }
    if (!core::IsValidTimelineEventProjection(event_projection)) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kInvalidTemplate,
                           "The event column projection is invalid."));
    }
    auto updated = *existing;
    updated.filter = std::move(filter);
    updated.event_projection = std::move(event_projection);
    const TimelineTemplateStore store{directory_};
    const auto result = store.Save(updated);
    if (!result.has_value()) {
        return std::unexpected(ServiceFailure(result.error()));
    }
    *existing = updated;
    return updated;
}

TimelineTemplateResult
TimelineTemplateService::Rename(std::string_view identifier, std::string name) {
    if (!loaded_) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotLoaded,
                           "The template catalog has not been loaded."));
    }
    auto *existing = FindMutable(identifier);
    if (existing == nullptr) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotFound,
                           "The template does not exist."));
    }
    const auto validation = ValidateName(name, identifier);
    if (!validation.has_value()) {
        return std::unexpected(validation.error());
    }
    auto renamed = *existing;
    renamed.name = std::move(name);
    const TimelineTemplateStore store{directory_};
    const auto result = store.Save(renamed);
    if (!result.has_value()) {
        return std::unexpected(ServiceFailure(result.error()));
    }
    *existing = renamed;
    Sort();
    return renamed;
}

TimelineTemplateResult
TimelineTemplateService::Duplicate(std::string_view identifier,
                                   std::string name) {
    if (!loaded_) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotLoaded,
                           "The template catalog has not been loaded."));
    }
    const auto *source = Find(identifier);
    if (source == nullptr) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotFound,
                           "The template does not exist."));
    }
    return Create(std::move(name), source->filter, source->event_projection);
}

TimelineTemplateMutationResult
TimelineTemplateService::Remove(std::string_view identifier) {
    if (!loaded_) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotLoaded,
                           "The template catalog has not been loaded."));
    }
    const auto position = std::ranges::find(templates_, identifier,
                                            &TimelineTemplate::identifier);
    if (position == templates_.end()) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotFound,
                           "The template does not exist."));
    }
    const TimelineTemplateStore store{directory_};
    const auto result = store.Remove(identifier);
    if (!result.has_value()) {
        return std::unexpected(ServiceFailure(result.error()));
    }
    templates_.erase(position);
    return {};
}

std::expected<void, TimelineTemplateFailure>
TimelineTemplateService::ValidateName(
    std::string_view name, std::string_view excluded_identifier) const {
    if (!loaded_) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNotLoaded,
                           "The template catalog has not been loaded."));
    }
    if (name.empty()) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kInvalidTemplate,
                           "The template name is empty."));
    }
    const auto duplicate = std::ranges::any_of(
        templates_, [name, excluded_identifier](const TimelineTemplate &value) {
            return value.identifier != excluded_identifier &&
                   value.name == name;
        });
    if (duplicate) {
        return std::unexpected(
            ServiceFailure(TimelineTemplateFailureKind::kNameConflict,
                           "The template name is already in use."));
    }
    return {};
}

TimelineTemplate *
TimelineTemplateService::FindMutable(std::string_view identifier) {
    const auto position = std::ranges::find(templates_, identifier,
                                            &TimelineTemplate::identifier);
    return position == templates_.end() ? nullptr : &*position;
}

void TimelineTemplateService::Sort(void) {
    std::ranges::sort(templates_, {}, &TimelineTemplate::name);
}

} // namespace edit_atlas::services
