#ifndef EDIT_ATLAS_PRESENTATION_TIMELINE_TEMPLATE_VIEW_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_TIMELINE_TEMPLATE_VIEW_MODEL_HPP_

#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/presentation/timeline_template_model.hpp>

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
#include <variant>
#include <vector>

namespace edit_atlas::presentation {

/// Explains why a template command was rejected before reaching persistence.
enum class TimelineTemplateCommandError {
    /// The active filter contains an invalid condition.
    kInvalidFilter,
    /// The selected event projection is empty, duplicated, or unknown.
    kInvalidProjection,
    /// The command requires an active template.
    kNoActiveTemplate,
};

/// A presentation-state rejection or a template-persistence failure.
using TimelineTemplateCommandFailure =
    std::variant<TimelineTemplateCommandError,
                 services::TimelineTemplateFailure>;

/// The result of a template mutation requested by a frontend.
using TimelineTemplateCommandResult =
    std::expected<void, TimelineTemplateCommandFailure>;

/// Owns reusable-template catalog and selection state for desktop frontends.
class TimelineTemplateViewModel final : public QObject {
    Q_OBJECT

  public:
    /// Creates template presentation state backed by the supplied directory.
    explicit TimelineTemplateViewModel(std::filesystem::path directory,
                                       QObject *parent = nullptr);
    /// Destroys the ViewModel and its template service.
    ~TimelineTemplateViewModel(void) override = default;

    /// ViewModels are non-copyable QObject owners.
    TimelineTemplateViewModel(const TimelineTemplateViewModel &) = delete;
    /// ViewModels are non-copy-assignable QObject owners.
    TimelineTemplateViewModel &
    operator=(const TimelineTemplateViewModel &) = delete;
    /// ViewModels are non-movable QObject owners.
    TimelineTemplateViewModel(TimelineTemplateViewModel &&) = delete;
    /// ViewModels are non-move-assignable QObject owners.
    TimelineTemplateViewModel &operator=(TimelineTemplateViewModel &&) = delete;

    /// Loads the persisted catalog and exposes recoverable file diagnostics.
    [[nodiscard]] services::TimelineTemplateLoadResult Load(void);
    /// Activates a saved template and applies its filter and projection.
    [[nodiscard]] bool SelectTemplate(std::string_view identifier);
    /// Selects no template and clears the active filter.
    void SelectNoTemplate(void);
    /// Restores the active template for a new timeline, or fresh defaults.
    void RestoreForTimeline(void);

    /// Synchronizes the current filter and whether it can be persisted.
    void SetFilterState(services::TimelineFilterQuery filter, bool valid);
    /// Replaces the current ordered export projection.
    [[nodiscard]] TimelineTemplateCommandResult
    SetEventProjection(std::vector<core::TimelineEventField> projection);

    /// Creates and activates a template from the current state.
    [[nodiscard]] TimelineTemplateCommandResult Create(std::string name);
    /// Replaces the active template contents with the current state.
    [[nodiscard]] TimelineTemplateCommandResult UpdateActive(void);
    /// Renames the active template.
    [[nodiscard]] TimelineTemplateCommandResult RenameActive(std::string name);
    /// Duplicates and activates the active template under a new name.
    [[nodiscard]] TimelineTemplateCommandResult
    DuplicateActive(std::string name);
    /// Removes the active template and selects no template.
    [[nodiscard]] TimelineTemplateCommandResult RemoveActive(void);

    /// Returns the loaded templates in deterministic display-name order.
    [[nodiscard]] std::span<const services::TimelineTemplate>
    Templates(void) const noexcept;
    /// Returns the Qt item model presenting the template choices.
    [[nodiscard]] TimelineTemplateModel &TemplateModel(void) noexcept;
    /// Returns the Qt item model presenting the template choices.
    [[nodiscard]] const TimelineTemplateModel &TemplateModel(void) const
        noexcept;
    /// Returns the active template identifier, or no value when none is active.
    [[nodiscard]] const std::optional<std::string> &
    ActiveIdentifier(void) const noexcept;
    /// Returns the active template, or null when none is active.
    [[nodiscard]] const services::TimelineTemplate *
    ActiveTemplate(void) const noexcept;
    /// Returns the current filter query.
    [[nodiscard]] const services::TimelineFilterQuery &
    FilterQuery(void) const noexcept;
    /// Returns whether the current filter can be persisted.
    [[nodiscard]] bool IsFilterValid(void) const noexcept;
    /// Returns the current ordered export projection.
    [[nodiscard]] std::span<const core::TimelineEventField>
    EventProjection(void) const noexcept;
    /// Returns whether current state differs from the active template.
    [[nodiscard]] bool IsModified(void) const noexcept;
    /// Notifies item-model consumers that localized display text changed.
    void Retranslate(void);

  signals:
    /// Reports that the loaded template catalog changed.
    void TemplatesChanged(void);
    /// Reports a change to the active template.
    void ActiveTemplateChanged(void);
    /// Reports a change to the current filter or its validity.
    void FilterStateChanged(void);
    /// Reports a change to the current ordered export fields.
    void EventProjectionChanged(void);
    /// Reports a change to `IsModified()`.
    void ModifiedChanged(void);

  private:
    [[nodiscard]] TimelineTemplateCommandResult
    ValidateWritableState(void) const;
    [[nodiscard]] TimelineTemplateCommandResult RequireActive(void) const;
    void ApplyTemplate(const services::TimelineTemplate &value);
    void RefreshTemplateModel(void);
    void RefreshModified(void);
    void SetActiveIdentifier(std::optional<std::string> identifier);

    services::TimelineTemplateService service_;
    TimelineTemplateModel template_model_;
    services::TimelineFilterQuery filter_;
    std::vector<core::TimelineEventField> event_projection_;
    std::optional<std::string> active_identifier_;
    bool filter_valid_ = true;
    bool modified_ = false;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TIMELINE_TEMPLATE_VIEW_MODEL_HPP_
