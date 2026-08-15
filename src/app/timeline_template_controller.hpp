#ifndef EDIT_ATLAS_APP_TIMELINE_TEMPLATE_CONTROLLER_HPP_
#define EDIT_ATLAS_APP_TIMELINE_TEMPLATE_CONTROLLER_HPP_

#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/presentation/timeline_template_view_model.hpp>

#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_template_service.hpp>

#include <QObject>
#include <QString>

#include <optional>
#include <span>
#include <string>
#include <vector>

class QWidget;

namespace edit_atlas::app {

class TimelineDocumentView;

/// Adapts timeline-template ViewModel commands to Qt Widgets interactions.
class TimelineTemplateController final : public QObject {
    Q_OBJECT

  public:
    TimelineTemplateController(TimelineDocumentView &view, QWidget &window,
                               QObject *parent = nullptr);
    ~TimelineTemplateController(void) override = default;

    TimelineTemplateController(const TimelineTemplateController &) = delete;
    TimelineTemplateController &
    operator=(const TimelineTemplateController &) = delete;
    TimelineTemplateController(TimelineTemplateController &&) = delete;
    TimelineTemplateController &
    operator=(TimelineTemplateController &&) = delete;

    /// Returns the current ordered export projection.
    [[nodiscard]] std::span<const core::TimelineEventField>
    EventProjection(void) const noexcept;

    /// Restores the active template, or fresh defaults when none is active.
    void RestoreForTimeline(void);

    /// Replaces the current export projection and refreshes the View.
    void
    SetEventProjection(std::vector<core::TimelineEventField> event_projection);

    /// Synchronizes the current filter and whether it can be persisted.
    void SetFilterState(services::TimelineFilterQuery filter, bool valid);

  private:
    void ApplyTemplate(const QString &identifier);
    void DeleteTemplate(void);
    void DuplicateTemplate(void);
    void EditExportColumns(void);
    void LoadTemplates(void);
    [[nodiscard]] std::optional<std::string>
    PromptForTemplateName(const QString &title, const QString &label,
                          const QString &initial);
    void RefreshTemplateState(void);
    void RenameTemplate(void);
    void SaveTemplate(void);
    void ShowCommandFailure(
        const QString &title,
        const presentation::TimelineTemplateCommandFailure &failure);
    void ShowInvalidFilter(const QString &title, const QString &description);
    void ShowServiceFailure(const QString &title,
                            const services::TimelineTemplateFailure &failure);
    void UpdateTemplate(void);

    TimelineDocumentView &view_;
    QWidget &window_;
    presentation::TimelineTemplateViewModel view_model_;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_TIMELINE_TEMPLATE_CONTROLLER_HPP_
