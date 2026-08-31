#include <edit_atlas/presentation/timeline_template_view_model.hpp>

#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_template.hpp>
#include <edit_atlas/services/timeline_template_service.hpp>

#include <QObject>

#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::presentation {
namespace {

[[nodiscard]] std::vector<core::TimelineEventField>
DefaultEventProjection(void) {
    const auto projection = core::DefaultTimelineEventProjection();
    return {projection.begin(), projection.end()};
}

[[nodiscard]] TimelineTemplateCommandFailure
CommandFailure(TimelineTemplateCommandError error) {
    return error;
}

[[nodiscard]] TimelineTemplateCommandFailure
ServiceFailure(services::TimelineTemplateFailure failure) {
    return failure;
}

} // namespace

TimelineTemplateViewModel::TimelineTemplateViewModel(
    std::filesystem::path directory, QObject *parent)
    : QObject{parent}, service_{std::move(directory)},
      event_projection_{DefaultEventProjection()} {
    connect(this, &TimelineTemplateViewModel::TemplatesChanged, this,
            &TimelineTemplateViewModel::RefreshTemplateModel);
    connect(this, &TimelineTemplateViewModel::ActiveTemplateChanged, this,
            &TimelineTemplateViewModel::RefreshTemplateModel);
    connect(this, &TimelineTemplateViewModel::ModifiedChanged, this,
            &TimelineTemplateViewModel::RefreshTemplateModel);
    RefreshTemplateModel();
}

services::TimelineTemplateLoadResult TimelineTemplateViewModel::Load(void) {
    auto result = service_.Load();
    if (!result.has_value()) {
        return result;
    }

    if (active_identifier_.has_value() &&
        service_.Find(*active_identifier_) == nullptr) {
        SetActiveIdentifier(std::nullopt);
    }
    emit TemplatesChanged();
    RefreshModified();
    return result;
}

bool TimelineTemplateViewModel::SelectTemplate(std::string_view identifier) {
    const auto *value = service_.Find(identifier);
    if (value == nullptr) {
        SelectNoTemplate();
        return false;
    }

    SetActiveIdentifier(value->identifier);
    ApplyTemplate(*value);
    return true;
}

void TimelineTemplateViewModel::SelectNoTemplate(void) {
    SetActiveIdentifier(std::nullopt);
    if (filter_ != services::TimelineFilterQuery{}) {
        filter_ = {};
        filter_valid_ = true;
        emit FilterStateChanged();
    } else if (!filter_valid_) {
        filter_valid_ = true;
        emit FilterStateChanged();
    }
    RefreshModified();
}

void TimelineTemplateViewModel::RestoreForTimeline(void) {
    if (active_identifier_.has_value()) {
        const auto *value = service_.Find(*active_identifier_);
        if (value != nullptr) {
            ApplyTemplate(*value);
            return;
        }
    }

    SetActiveIdentifier(std::nullopt);
    const auto default_projection = DefaultEventProjection();
    const bool filter_changed =
        filter_ != services::TimelineFilterQuery{} || !filter_valid_;
    const bool projection_changed = event_projection_ != default_projection;
    filter_ = {};
    filter_valid_ = true;
    event_projection_ = default_projection;
    if (filter_changed) {
        emit FilterStateChanged();
    }
    if (projection_changed) {
        emit EventProjectionChanged();
    }
    RefreshModified();
}

void TimelineTemplateViewModel::SetFilterState(
    services::TimelineFilterQuery filter, bool valid) {
    if (filter_ == filter && filter_valid_ == valid) {
        return;
    }
    filter_ = std::move(filter);
    filter_valid_ = valid;
    emit FilterStateChanged();
    RefreshModified();
}

TimelineTemplateCommandResult TimelineTemplateViewModel::SetEventProjection(
    std::vector<core::TimelineEventField> projection) {
    if (!core::IsValidTimelineEventProjection(projection)) {
        return std::unexpected{
            CommandFailure(TimelineTemplateCommandError::kInvalidProjection)};
    }
    if (event_projection_ == projection) {
        return {};
    }
    event_projection_ = std::move(projection);
    emit EventProjectionChanged();
    RefreshModified();
    return {};
}

TimelineTemplateCommandResult
TimelineTemplateViewModel::Create(std::string name) {
    const auto validation = ValidateWritableState();
    if (!validation.has_value()) {
        return validation;
    }
    auto result = service_.Create(std::move(name), filter_, event_projection_);
    if (!result.has_value()) {
        return std::unexpected{ServiceFailure(std::move(result.error()))};
    }
    SetActiveIdentifier(result->identifier);
    emit TemplatesChanged();
    RefreshModified();
    return {};
}

TimelineTemplateCommandResult TimelineTemplateViewModel::UpdateActive(void) {
    const auto active = RequireActive();
    if (!active.has_value()) {
        return active;
    }
    const auto validation = ValidateWritableState();
    if (!validation.has_value()) {
        return validation;
    }
    auto result =
        service_.Update(*active_identifier_, filter_, event_projection_);
    if (!result.has_value()) {
        return std::unexpected{ServiceFailure(std::move(result.error()))};
    }
    emit TemplatesChanged();
    RefreshModified();
    return {};
}

TimelineTemplateCommandResult
TimelineTemplateViewModel::RenameActive(std::string name) {
    const auto active = RequireActive();
    if (!active.has_value()) {
        return active;
    }
    auto result = service_.Rename(*active_identifier_, std::move(name));
    if (!result.has_value()) {
        return std::unexpected{ServiceFailure(std::move(result.error()))};
    }
    emit TemplatesChanged();
    RefreshModified();
    return {};
}

TimelineTemplateCommandResult
TimelineTemplateViewModel::DuplicateActive(std::string name) {
    const auto active = RequireActive();
    if (!active.has_value()) {
        return active;
    }
    auto result = service_.Duplicate(*active_identifier_, std::move(name));
    if (!result.has_value()) {
        return std::unexpected{ServiceFailure(std::move(result.error()))};
    }
    SetActiveIdentifier(result->identifier);
    emit TemplatesChanged();
    RefreshModified();
    return {};
}

TimelineTemplateCommandResult TimelineTemplateViewModel::RemoveActive(void) {
    const auto active = RequireActive();
    if (!active.has_value()) {
        return active;
    }
    auto result = service_.Remove(*active_identifier_);
    if (!result.has_value()) {
        return std::unexpected{ServiceFailure(std::move(result.error()))};
    }
    SelectNoTemplate();
    emit TemplatesChanged();
    return {};
}

std::span<const services::TimelineTemplate>
TimelineTemplateViewModel::Templates(void) const noexcept {
    return service_.Templates();
}

TimelineTemplateModel &TimelineTemplateViewModel::TemplateModel(void) noexcept {
    return template_model_;
}

const TimelineTemplateModel &
TimelineTemplateViewModel::TemplateModel(void) const noexcept {
    return template_model_;
}

const std::optional<std::string> &
TimelineTemplateViewModel::ActiveIdentifier(void) const noexcept {
    return active_identifier_;
}

const services::TimelineTemplate *
TimelineTemplateViewModel::ActiveTemplate(void) const noexcept {
    return active_identifier_.has_value() ? service_.Find(*active_identifier_)
                                          : nullptr;
}

const services::TimelineFilterQuery &
TimelineTemplateViewModel::FilterQuery(void) const noexcept {
    return filter_;
}

bool TimelineTemplateViewModel::IsFilterValid(void) const noexcept {
    return filter_valid_;
}

std::span<const core::TimelineEventField>
TimelineTemplateViewModel::EventProjection(void) const noexcept {
    return event_projection_;
}

bool TimelineTemplateViewModel::IsModified(void) const noexcept {
    return modified_;
}

void TimelineTemplateViewModel::Retranslate(void) {
    template_model_.Retranslate();
}

TimelineTemplateCommandResult
TimelineTemplateViewModel::ValidateWritableState(void) const {
    if (!filter_valid_) {
        return std::unexpected{
            CommandFailure(TimelineTemplateCommandError::kInvalidFilter)};
    }
    if (!core::IsValidTimelineEventProjection(event_projection_)) {
        return std::unexpected{
            CommandFailure(TimelineTemplateCommandError::kInvalidProjection)};
    }
    return {};
}

TimelineTemplateCommandResult
TimelineTemplateViewModel::RequireActive(void) const {
    if (!active_identifier_.has_value() || ActiveTemplate() == nullptr) {
        return std::unexpected{
            CommandFailure(TimelineTemplateCommandError::kNoActiveTemplate)};
    }
    return {};
}

void TimelineTemplateViewModel::ApplyTemplate(
    const services::TimelineTemplate &value) {
    const bool filter_changed = filter_ != value.filter || !filter_valid_;
    const bool projection_changed = event_projection_ != value.event_projection;
    filter_ = value.filter;
    filter_valid_ = true;
    event_projection_ = value.event_projection;
    if (filter_changed) {
        emit FilterStateChanged();
    }
    if (projection_changed) {
        emit EventProjectionChanged();
    }
    RefreshModified();
}

void TimelineTemplateViewModel::RefreshModified(void) {
    const auto *active = ActiveTemplate();
    const bool modified =
        active != nullptr && (active->filter != filter_ ||
                              active->event_projection != event_projection_);
    if (modified_ == modified) {
        return;
    }
    modified_ = modified;
    emit ModifiedChanged();
}

void TimelineTemplateViewModel::RefreshTemplateModel(void) {
    const auto active_identifier =
        active_identifier_.has_value()
            ? std::optional<std::string_view>{*active_identifier_}
            : std::nullopt;
    template_model_.SetTemplates(service_.Templates(), active_identifier,
                                 modified_);
}

void TimelineTemplateViewModel::SetActiveIdentifier(
    std::optional<std::string> identifier) {
    if (active_identifier_ == identifier) {
        return;
    }
    active_identifier_ = std::move(identifier);
    emit ActiveTemplateChanged();
}

} // namespace edit_atlas::presentation
