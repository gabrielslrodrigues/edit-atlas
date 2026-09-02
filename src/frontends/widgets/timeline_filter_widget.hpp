#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_FILTER_WIDGET_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_FILTER_WIDGET_HPP_

#include <edit_atlas/presentation/timeline_filter_model.hpp>

#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_template.hpp>

#include <QString>
#include <QVariant>
#include <QWidget>

#include <optional>
#include <span>
#include <string_view>
#include <vector>

class QAction;
class QComboBox;
class QLabel;
class QIntValidator;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace edit_atlas::frontends::widgets {

/// Edits a presentation-independent timeline filter query.
class TimelineFilterWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit TimelineFilterWidget(QWidget *parent = nullptr);
    ~TimelineFilterWidget(void) override = default;

    TimelineFilterWidget(const TimelineFilterWidget &) = delete;
    TimelineFilterWidget &operator=(const TimelineFilterWidget &) = delete;
    TimelineFilterWidget(TimelineFilterWidget &&) = delete;
    TimelineFilterWidget &operator=(TimelineFilterWidget &&) = delete;

    /// Attaches the shared editable filter presentation model.
    void SetModel(presentation::TimelineFilterModel &model);
    /// Clears every condition and restores all-event matching.
    void Clear(void);
    /// Returns the current UI-independent query.
    [[nodiscard]] services::TimelineFilterQuery Query(void) const;
    /// Replaces the edited query without depending on persisted UI text.
    void SetQuery(const services::TimelineFilterQuery &query);
    /// Shows an inline validation error, or clears it for empty text.
    void SetError(QString error);
    /// Rebuilds the template selector and its active state.
    void SetTemplates(std::span<const services::TimelineTemplate> templates,
                      std::optional<std::string_view> active_identifier,
                      bool modified);
    /// Updates every user-facing string after a language change.
    void RetranslateUi(void);

  signals:
    void queryChanged(void);
    void deleteTemplateRequested(void);
    void duplicateTemplateRequested(void);
    void editColumnsRequested(void);
    void renameTemplateRequested(void);
    void saveTemplateRequested(void);
    void templateSelected(const QString &identifier);
    void updateTemplateRequested(void);

  private:
    struct ConditionRow final {
        QWidget *widget;
        QComboBox *field;
        QLineEdit *text;
        QComboBox *track_kind;
        QComboBox *edit_type;
        QIntValidator *duration_validator;
        QToolButton *match_case;
        QToolButton *match_whole_word;
        QToolButton *regular_expression;
        QPushButton *remove;
    };

    void AddCondition(bool focus);
    void AddConditionRow(int model_row);
    void BuildUi(void);
    void ClearConditionRows(void);
    void PopulateEditTypes(ConditionRow &row);
    void PopulateFields(ConditionRow &row);
    void PopulateTrackKinds(ConditionRow &row);
    void RebuildConditionRows(void);
    void RemoveCondition(QWidget *row_widget);
    void SetConditionData(QWidget *row_widget, const QVariant &value, int role);
    void SynchronizeCombination(void);
    void SynchronizeConditionRow(ConditionRow &row, int model_row);
    void UpdateAccessibleIdentifiers(void);
    void UpdateConditionEditor(ConditionRow &row, int model_row);
    void UpdateConditionsViewportHeight(void);
    void UpdateRemoveButtons(void);
    void UpdateTemplateControls(void);

    QLabel *combination_label_ = nullptr;
    QLabel *error_label_ = nullptr;
    QLabel *filter_title_label_ = nullptr;
    QLabel *template_label_ = nullptr;
    QLabel *template_status_ = nullptr;
    QComboBox *combination_ = nullptr;
    QComboBox *template_selector_ = nullptr;
    QPushButton *add_condition_button_ = nullptr;
    QPushButton *clear_button_ = nullptr;
    QPushButton *template_primary_button_ = nullptr;
    QToolButton *template_actions_button_ = nullptr;
    QAction *save_template_action_ = nullptr;
    QAction *edit_columns_action_ = nullptr;
    QAction *rename_template_action_ = nullptr;
    QAction *duplicate_template_action_ = nullptr;
    QAction *delete_template_action_ = nullptr;
    QScrollArea *conditions_scroll_ = nullptr;
    QWidget *conditions_container_ = nullptr;
    QVBoxLayout *conditions_layout_ = nullptr;
    presentation::TimelineFilterModel *model_ = nullptr;
    bool has_active_template_ = false;
    bool template_modified_ = false;
    std::vector<ConditionRow> rows_;
};

} // namespace edit_atlas::frontends::widgets

#endif // EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_FILTER_WIDGET_HPP_
