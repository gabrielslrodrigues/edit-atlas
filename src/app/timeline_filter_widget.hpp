#ifndef EDIT_ATLAS_APP_TIMELINE_FILTER_WIDGET_HPP_
#define EDIT_ATLAS_APP_TIMELINE_FILTER_WIDGET_HPP_

#include <edit_atlas/services/timeline_filter.hpp>

#include <QString>
#include <QWidget>

#include <vector>

class QComboBox;
class QLabel;
class QIntValidator;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace edit_atlas::app {

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

    /// Clears every condition and restores all-event matching.
    void Clear(void);
    /// Returns the current UI-independent query.
    [[nodiscard]] services::TimelineFilterQuery Query(void) const;
    /// Shows an inline validation error, or clears it for empty text.
    void SetError(QString error);
    /// Updates every user-facing string after a language change.
    void RetranslateUi(void);

  signals:
    void QueryChanged(void);

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

    void AddCondition(void);
    void BuildUi(void);
    void PopulateEditTypes(ConditionRow &row);
    void PopulateFields(ConditionRow &row);
    void PopulateTrackKinds(ConditionRow &row);
    void RemoveCondition(QWidget *row_widget);
    void UpdateConditionEditor(ConditionRow &row);
    void UpdateRemoveButtons(void);

    QLabel *combination_label_ = nullptr;
    QLabel *error_label_ = nullptr;
    QComboBox *combination_ = nullptr;
    QPushButton *add_condition_button_ = nullptr;
    QPushButton *clear_button_ = nullptr;
    QScrollArea *conditions_scroll_ = nullptr;
    QWidget *conditions_container_ = nullptr;
    QVBoxLayout *conditions_layout_ = nullptr;
    std::vector<ConditionRow> rows_;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_TIMELINE_FILTER_WIDGET_HPP_
