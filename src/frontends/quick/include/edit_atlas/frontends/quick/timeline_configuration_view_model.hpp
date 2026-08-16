#ifndef EDIT_ATLAS_FRONTENDS_QUICK_TIMELINE_CONFIGURATION_VIEW_MODEL_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_TIMELINE_CONFIGURATION_VIEW_MODEL_HPP_

#include <edit_atlas/presentation/timeline_document_view_model.hpp>
#include <edit_atlas/presentation/timeline_event_projection_model.hpp>
#include <edit_atlas/presentation/timeline_filter_model.hpp>
#include <edit_atlas/presentation/timeline_template_view_model.hpp>

#include <QAbstractItemModel>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

#include <filesystem>

namespace edit_atlas::frontends::quick {

/// Adapts shared filter, template, and projection state for QML.
class TimelineConfigurationViewModel final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(TimelineConfiguration)
    QML_UNCREATABLE("Provided by the Edit Atlas application")

    Q_PROPERTY(QAbstractItemModel *filterModel READ FilterModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *templateModel READ TemplateModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *eventProjectionModel READ
                   EventProjectionModel CONSTANT)
    Q_PROPERTY(int activeTemplateRow READ ActiveTemplateRow NOTIFY
                   TemplateStateChanged)
    Q_PROPERTY(bool hasActiveTemplate READ HasActiveTemplate NOTIFY
                   TemplateStateChanged)
    Q_PROPERTY(bool templateModified READ IsTemplateModified NOTIFY
                   TemplateStateChanged)
    Q_PROPERTY(bool filterValid READ IsFilterValid NOTIFY FilterStateChanged)
    Q_PROPERTY(QString filterErrorText READ FilterErrorText NOTIFY
                   FilterStateChanged)
    Q_PROPERTY(bool eventProjectionValid READ IsEventProjectionValid NOTIFY
                   EventProjectionStateChanged)
    Q_PROPERTY(int filterCombination READ FilterCombination WRITE
                   SetFilterCombination NOTIFY FilterStateChanged)
    Q_PROPERTY(QStringList filterCombinationNames READ FilterCombinationNames
                   NOTIFY DisplayTextChanged)
    Q_PROPERTY(QStringList filterFieldNames READ FilterFieldNames NOTIFY
                   DisplayTextChanged)
    Q_PROPERTY(QStringList filterTrackKindNames READ FilterTrackKindNames
                   NOTIFY DisplayTextChanged)
    Q_PROPERTY(QStringList filterEditTypeNames READ FilterEditTypeNames NOTIFY
                   DisplayTextChanged)

  public:
    /// Creates configuration state backed by \p template_directory.
    TimelineConfigurationViewModel(
        presentation::TimelineDocumentViewModel &document_view_model,
        std::filesystem::path template_directory, QObject *parent = nullptr);
    /// Destroys the editable models and template catalog.
    ~TimelineConfigurationViewModel(void) override = default;

    TimelineConfigurationViewModel(const TimelineConfigurationViewModel &) =
        delete;
    TimelineConfigurationViewModel &
    operator=(const TimelineConfigurationViewModel &) = delete;
    TimelineConfigurationViewModel(TimelineConfigurationViewModel &&) =
        delete;
    TimelineConfigurationViewModel &
    operator=(TimelineConfigurationViewModel &&) = delete;

    /// Returns the editable timeline-filter item model exposed to QML.
    [[nodiscard]] QAbstractItemModel *FilterModel(void) noexcept;
    /// Returns the saved-template catalog exposed to QML.
    [[nodiscard]] QAbstractItemModel *TemplateModel(void) noexcept;
    /// Returns the editable ordered event-column model exposed to QML.
    [[nodiscard]] QAbstractItemModel *EventProjectionModel(void) noexcept;
    /// Returns the active template row, including the no-template row.
    [[nodiscard]] int ActiveTemplateRow(void) const noexcept;
    /// Returns whether a saved template is active.
    [[nodiscard]] bool HasActiveTemplate(void) const noexcept;
    /// Returns whether editable state differs from the active template.
    [[nodiscard]] bool IsTemplateModified(void) const noexcept;
    /// Returns whether the active filter can be applied and persisted.
    [[nodiscard]] bool IsFilterValid(void) const noexcept;
    /// Returns a localized explanation for the active filter error.
    [[nodiscard]] QString FilterErrorText(void) const;
    /// Returns whether at least one export event column is selected.
    [[nodiscard]] bool IsEventProjectionValid(void) const noexcept;
    /// Returns how non-empty filter conditions are combined.
    [[nodiscard]] int FilterCombination(void) const noexcept;
    /// Selects how non-empty filter conditions are combined.
    void SetFilterCombination(int combination);
    /// Returns localized filter-combination choices.
    [[nodiscard]] QStringList FilterCombinationNames(void) const;
    /// Returns localized filter-field choices.
    [[nodiscard]] QStringList FilterFieldNames(void) const;
    /// Returns localized track-kind choices.
    [[nodiscard]] QStringList FilterTrackKindNames(void) const;
    /// Returns localized edit-type choices.
    [[nodiscard]] QStringList FilterEditTypeNames(void) const;

    /// Restores saved or default configuration for a new timeline.
    void RestoreForTimeline(void);
    /// Notifies model consumers that localized display text changed.
    void Retranslate(void);

    /// Adds one editable filter condition.
    Q_INVOKABLE void AddFilterCondition(void);
    /// Removes one editable filter condition when another row remains.
    Q_INVOKABLE void RemoveFilterCondition(int row);
    /// Restores an empty filter.
    Q_INVOKABLE void ClearFilter(void);
    /// Selects a saved template row or the explicit no-template row.
    Q_INVOKABLE void SelectTemplateRow(int row);
    /// Creates and activates a template from current editable state.
    Q_INVOKABLE bool CreateTemplate(const QString &name);
    /// Replaces the active template with current editable state.
    Q_INVOKABLE bool UpdateActiveTemplate(void);
    /// Renames the active template.
    Q_INVOKABLE bool RenameActiveTemplate(const QString &name);
    /// Duplicates and activates the active template.
    Q_INVOKABLE bool DuplicateActiveTemplate(const QString &name);
    /// Removes the active template and selects no template.
    Q_INVOKABLE bool RemoveActiveTemplate(void);
    /// Selects or excludes one event-column row.
    Q_INVOKABLE void SetEventProjectionSelected(int row, bool selected);
    /// Moves one event-column row upward when possible.
    Q_INVOKABLE void MoveEventProjectionUp(int row);
    /// Moves one event-column row downward when possible.
    Q_INVOKABLE void MoveEventProjectionDown(int row);

  signals:
    /// Reports a change to the editable filter or its validity.
    void FilterStateChanged(void);
    /// Reports a change to saved-template selection or modified state.
    void TemplateStateChanged(void);
    /// Reports a change to event-column ordering, selection, or validity.
    void EventProjectionStateChanged(void);
    /// Reports that localized choice text changed.
    void DisplayTextChanged(void);

  private:
    void HandleFilterQueryChanged(void);
    void HandleTemplateStateChanged(void);
    void HandleEventProjectionChanged(void);
    void SynchronizeTemplateState(void);

    presentation::TimelineDocumentViewModel &document_view_model_;
    presentation::TimelineFilterModel filter_model_;
    presentation::TimelineTemplateViewModel template_view_model_;
    presentation::TimelineEventProjectionModel event_projection_model_;
    bool synchronizing_template_state_ = false;
};

} // namespace edit_atlas::frontends::quick

#endif // EDIT_ATLAS_FRONTENDS_QUICK_TIMELINE_CONFIGURATION_VIEW_MODEL_HPP_
