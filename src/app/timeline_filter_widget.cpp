#include "timeline_filter_widget.hpp"

#include <QAbstractScrollArea>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <Qt>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace edit_atlas::app {
namespace {

enum class Field {
    kEventIdentifier,
    kReel,
    kTrackKind,
    kTrackIdentifier,
    kEditType,
    kClip,
    kSourceIn,
    kSourceOut,
    kRecordIn,
    kRecordOut,
    kDuration,
    kComments,
};

template <typename Enum> [[nodiscard]] int EnumData(Enum value) {
    return static_cast<int>(value);
}

[[nodiscard]] std::string Utf8(const QString &text) {
    const auto utf8 = text.toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

[[nodiscard]] services::TimelineTextFilterField TextFilterField(Field field) {
    switch (field) {
    case Field::kEventIdentifier:
        return services::TimelineTextFilterField::kEventIdentifier;
    case Field::kReel:
        return services::TimelineTextFilterField::kReel;
    case Field::kTrackIdentifier:
        return services::TimelineTextFilterField::kTrackIdentifier;
    case Field::kClip:
        return services::TimelineTextFilterField::kClip;
    case Field::kComments:
        return services::TimelineTextFilterField::kComments;
    case Field::kTrackKind:
    case Field::kEditType:
    case Field::kSourceIn:
    case Field::kSourceOut:
    case Field::kRecordIn:
    case Field::kRecordOut:
    case Field::kDuration:
        std::unreachable();
    }
    std::unreachable();
}

[[nodiscard]] services::TimelineTimecodeFilterField
TimecodeFilterField(Field field) {
    switch (field) {
    case Field::kSourceIn:
        return services::TimelineTimecodeFilterField::kSourceIn;
    case Field::kSourceOut:
        return services::TimelineTimecodeFilterField::kSourceOut;
    case Field::kRecordIn:
        return services::TimelineTimecodeFilterField::kRecordIn;
    case Field::kRecordOut:
        return services::TimelineTimecodeFilterField::kRecordOut;
    case Field::kEventIdentifier:
    case Field::kReel:
    case Field::kTrackKind:
    case Field::kTrackIdentifier:
    case Field::kEditType:
    case Field::kClip:
    case Field::kDuration:
    case Field::kComments:
        std::unreachable();
    }
    std::unreachable();
}

} // namespace

TimelineFilterWidget::TimelineFilterWidget(QWidget *parent) : QWidget{parent} {
    BuildUi();
    AddCondition();
    RetranslateUi();
}

void TimelineFilterWidget::Clear(void) {
    const QSignalBlocker combination_blocker{combination_};
    combination_->setCurrentIndex(0);
    for (const auto &row : rows_) {
        conditions_layout_->removeWidget(row.widget);
        row.widget->hide();
        row.widget->deleteLater();
    }
    rows_.clear();
    SetError({});
    AddCondition();
    emit QueryChanged();
}

services::TimelineFilterQuery TimelineFilterWidget::Query(void) const {
    services::TimelineFilterQuery query{
        .combination = static_cast<services::TimelineFilterCombination>(
            combination_->currentData().toInt()),
        .conditions = {},
    };
    query.conditions.reserve(rows_.size());
    for (const auto &row : rows_) {
        const auto field = static_cast<Field>(row.field->currentData().toInt());
        if (field == Field::kTrackKind) {
            query.conditions.emplace_back(
                services::TimelineTrackKindFilterCondition{
                    .track_kind = static_cast<core::TrackKind>(
                        row.track_kind->currentData().toInt()),
                });
        } else if (field == Field::kEditType) {
            query.conditions.emplace_back(
                services::TimelineEditTypeFilterCondition{
                    .edit_type = static_cast<core::EditType>(
                        row.edit_type->currentData().toInt()),
                });
        } else if (field == Field::kDuration) {
            bool valid = false;
            const auto frames = row.text->text().toLongLong(&valid);
            std::optional<std::int64_t> duration;
            if (valid) {
                duration = static_cast<std::int64_t>(frames);
            }
            query.conditions.emplace_back(
                services::TimelineDurationFilterCondition{
                    .frames = duration,
                });
        } else if (field == Field::kSourceIn || field == Field::kSourceOut ||
                   field == Field::kRecordIn || field == Field::kRecordOut) {
            query.conditions.emplace_back(
                services::TimelineTimecodeFilterCondition{
                    .field = TimecodeFilterField(field),
                    .timecode = Utf8(row.text->text()),
                });
        } else {
            query.conditions.emplace_back(services::TimelineTextFilterCondition{
                .field = TextFilterField(field),
                .text = Utf8(row.text->text()),
                .match_case = row.match_case->isChecked(),
                .match_whole_word = row.match_whole_word->isChecked(),
                .regular_expression = row.regular_expression->isChecked(),
            });
        }
    }
    return query;
}

void TimelineFilterWidget::SetError(QString error) {
    error_label_->setText(std::move(error));
    error_label_->setVisible(!error_label_->text().isEmpty());
}

void TimelineFilterWidget::RetranslateUi(void) {
    combination_label_->setText(tr("Match"));
    const auto combination = combination_->currentData();
    const QSignalBlocker combination_blocker{combination_};
    combination_->clear();
    combination_->addItem(tr("All conditions"),
                          EnumData(services::TimelineFilterCombination::kAll));
    combination_->addItem(tr("Any condition"),
                          EnumData(services::TimelineFilterCombination::kAny));
    const auto combination_index = combination_->findData(combination);
    combination_->setCurrentIndex(combination_index < 0 ? 0
                                                        : combination_index);
    combination_->setAccessibleName(tr("Filter combination"));
    add_condition_button_->setText(tr("Add condition"));
    clear_button_->setText(tr("Clear filters"));
    conditions_scroll_->setAccessibleName(tr("Filter conditions"));

    for (auto &row : rows_) {
        PopulateFields(row);
        PopulateTrackKinds(row);
        PopulateEditTypes(row);
        row.match_case->setToolTip(tr("Match case"));
        row.match_case->setAccessibleName(tr("Match case"));
        row.match_whole_word->setToolTip(tr("Match whole word"));
        row.match_whole_word->setAccessibleName(tr("Match whole word"));
        row.regular_expression->setToolTip(tr("Use regular expression"));
        row.regular_expression->setAccessibleName(tr("Use regular expression"));
        row.remove->setText(tr("Remove"));
        UpdateConditionEditor(row);
    }
}

void TimelineFilterWidget::AddCondition(void) {
    auto *widget = new QWidget{conditions_container_};
    auto *layout = new QHBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    ConditionRow row{
        .widget = widget,
        .field = new QComboBox{widget},
        .text = new QLineEdit{widget},
        .track_kind = new QComboBox{widget},
        .edit_type = new QComboBox{widget},
        .duration_validator =
            new QIntValidator{0, std::numeric_limits<int>::max(), widget},
        .match_case = new QToolButton{widget},
        .match_whole_word = new QToolButton{widget},
        .regular_expression = new QToolButton{widget},
        .remove = new QPushButton{widget},
    };
    row.field->setMinimumContentsLength(12);
    row.text->setClearButtonEnabled(true);
    row.match_case->setText(QStringLiteral("Aa"));
    row.match_case->setCheckable(true);
    row.match_case->setObjectName(QStringLiteral("matchCaseButton"));
    row.match_whole_word->setText(QStringLiteral("[ab]"));
    row.match_whole_word->setCheckable(true);
    row.match_whole_word->setObjectName(QStringLiteral("matchWholeWordButton"));
    row.regular_expression->setText(QStringLiteral(".*"));
    row.regular_expression->setCheckable(true);
    row.regular_expression->setObjectName(
        QStringLiteral("regularExpressionButton"));
    row.remove->setObjectName(QStringLiteral("removeFilterConditionButton"));
    layout->addWidget(row.field);
    layout->addWidget(row.text, 1);
    layout->addWidget(row.track_kind, 1);
    layout->addWidget(row.edit_type, 1);
    layout->addWidget(row.match_case);
    layout->addWidget(row.match_whole_word);
    layout->addWidget(row.regular_expression);
    layout->addWidget(row.remove);
    conditions_layout_->addWidget(widget);
    rows_.emplace_back(row);

    connect(row.field, &QComboBox::currentIndexChanged, this,
            [this, widget](int) {
                const auto match =
                    std::ranges::find(rows_, widget, &ConditionRow::widget);
                if (match != rows_.end()) {
                    UpdateConditionEditor(*match);
                    emit QueryChanged();
                }
            });
    connect(row.text, &QLineEdit::textChanged, this,
            [this](const QString &) { emit QueryChanged(); });
    connect(row.edit_type, &QComboBox::currentIndexChanged, this,
            [this](int) { emit QueryChanged(); });
    connect(row.track_kind, &QComboBox::currentIndexChanged, this,
            [this](int) { emit QueryChanged(); });
    connect(row.match_case, &QToolButton::toggled, this,
            [this](bool) { emit QueryChanged(); });
    connect(row.match_whole_word, &QToolButton::toggled, this,
            [this](bool) { emit QueryChanged(); });
    connect(row.regular_expression, &QToolButton::toggled, this,
            [this, widget](bool) {
                const auto match =
                    std::ranges::find(rows_, widget, &ConditionRow::widget);
                if (match != rows_.end()) {
                    UpdateConditionEditor(*match);
                    emit QueryChanged();
                }
            });
    connect(row.remove, &QPushButton::clicked, this,
            [this, widget](void) { RemoveCondition(widget); });

    PopulateFields(rows_.back());
    PopulateTrackKinds(rows_.back());
    PopulateEditTypes(rows_.back());
    rows_.back().match_case->setToolTip(tr("Match case"));
    rows_.back().match_case->setAccessibleName(tr("Match case"));
    rows_.back().match_whole_word->setToolTip(tr("Match whole word"));
    rows_.back().match_whole_word->setAccessibleName(tr("Match whole word"));
    rows_.back().regular_expression->setToolTip(tr("Use regular expression"));
    rows_.back().regular_expression->setAccessibleName(
        tr("Use regular expression"));
    rows_.back().remove->setText(tr("Remove"));
    UpdateConditionEditor(rows_.back());
    UpdateRemoveButtons();
    conditions_layout_->activate();
    if (rows_.size() == 1) {
        conditions_scroll_->setMinimumHeight(widget->sizeHint().height());
    }
    rows_.back().field->setFocus(Qt::OtherFocusReason);
    conditions_scroll_->ensureWidgetVisible(rows_.back().field, 0, 6);
}

void TimelineFilterWidget::BuildUi(void) {
    auto *root_layout = new QVBoxLayout{this};
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(6);

    auto *toolbar_layout = new QHBoxLayout;
    combination_label_ = new QLabel{this};
    toolbar_layout->addWidget(combination_label_);
    combination_ = new QComboBox{this};
    toolbar_layout->addWidget(combination_);
    toolbar_layout->addStretch(1);
    add_condition_button_ = new QPushButton{this};
    toolbar_layout->addWidget(add_condition_button_);
    clear_button_ = new QPushButton{this};
    toolbar_layout->addWidget(clear_button_);
    root_layout->addLayout(toolbar_layout);

    conditions_scroll_ = new QScrollArea{this};
    conditions_scroll_->setObjectName(
        QStringLiteral("filterConditionsScrollArea"));
    conditions_scroll_->setFrameShape(QFrame::NoFrame);
    conditions_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    conditions_scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    conditions_scroll_->setWidgetResizable(true);
    conditions_scroll_->setSizeAdjustPolicy(
        QAbstractScrollArea::AdjustToContents);
    conditions_scroll_->viewport()->setAutoFillBackground(false);

    conditions_container_ = new QWidget{conditions_scroll_};
    conditions_container_->setObjectName(
        QStringLiteral("filterConditionsContainer"));
    conditions_container_->setAutoFillBackground(false);
    conditions_layout_ = new QVBoxLayout{conditions_container_};
    conditions_layout_->setContentsMargins(0, 0, 0, 0);
    conditions_layout_->setSpacing(6);
    conditions_layout_->setSizeConstraint(QLayout::SetMinAndMaxSize);
    conditions_layout_->setAlignment(Qt::AlignTop);
    conditions_scroll_->setWidget(conditions_container_);
    root_layout->addWidget(conditions_scroll_, 1);

    error_label_ = new QLabel{this};
    error_label_->setObjectName(QStringLiteral("filterErrorLabel"));
    error_label_->setTextFormat(Qt::PlainText);
    error_label_->setWordWrap(true);
    error_label_->setVisible(false);
    root_layout->addWidget(error_label_);

    connect(combination_, &QComboBox::currentIndexChanged, this,
            [this](int) { emit QueryChanged(); });
    connect(add_condition_button_, &QPushButton::clicked, this,
            [this](void) { AddCondition(); });
    connect(clear_button_, &QPushButton::clicked, this,
            &TimelineFilterWidget::Clear);
}

void TimelineFilterWidget::PopulateEditTypes(ConditionRow &row) {
    const auto edit_type = row.edit_type->currentData();
    const QSignalBlocker blocker{row.edit_type};
    row.edit_type->clear();
    row.edit_type->addItem(tr("Cut"), EnumData(core::EditType::kCut));
    row.edit_type->addItem(tr("Dissolve"), EnumData(core::EditType::kDissolve));
    row.edit_type->addItem(tr("Wipe"), EnumData(core::EditType::kWipe));
    row.edit_type->addItem(tr("Key"), EnumData(core::EditType::kKey));
    row.edit_type->addItem(tr("Other"), EnumData(core::EditType::kOther));
    const auto index = row.edit_type->findData(edit_type);
    row.edit_type->setCurrentIndex(index < 0 ? 0 : index);
}

void TimelineFilterWidget::PopulateFields(ConditionRow &row) {
    const auto field = row.field->currentData();
    const QSignalBlocker blocker{row.field};
    row.field->clear();
    row.field->addItem(tr("Event"), EnumData(Field::kEventIdentifier));
    row.field->addItem(tr("Reel"), EnumData(Field::kReel));
    row.field->addItem(tr("Track type"), EnumData(Field::kTrackKind));
    row.field->addItem(tr("Track ID"), EnumData(Field::kTrackIdentifier));
    row.field->addItem(tr("Edit type"), EnumData(Field::kEditType));
    row.field->addItem(tr("Clip"), EnumData(Field::kClip));
    row.field->addItem(tr("Source In"), EnumData(Field::kSourceIn));
    row.field->addItem(tr("Source Out"), EnumData(Field::kSourceOut));
    row.field->addItem(tr("Record In"), EnumData(Field::kRecordIn));
    row.field->addItem(tr("Record Out"), EnumData(Field::kRecordOut));
    row.field->addItem(tr("Duration"), EnumData(Field::kDuration));
    row.field->addItem(tr("Comments"), EnumData(Field::kComments));
    const auto index = row.field->findData(field);
    row.field->setCurrentIndex(index < 0 ? 0 : index);
}

void TimelineFilterWidget::PopulateTrackKinds(ConditionRow &row) {
    const auto track_kind = row.track_kind->currentData();
    const QSignalBlocker blocker{row.track_kind};
    row.track_kind->clear();
    row.track_kind->addItem(tr("Video"), EnumData(core::TrackKind::kVideo));
    row.track_kind->addItem(tr("Audio"), EnumData(core::TrackKind::kAudio));
    row.track_kind->addItem(tr("Data"), EnumData(core::TrackKind::kData));
    row.track_kind->addItem(tr("Other"), EnumData(core::TrackKind::kOther));
    const auto index = row.track_kind->findData(track_kind);
    row.track_kind->setCurrentIndex(index < 0 ? 0 : index);
}

void TimelineFilterWidget::RemoveCondition(QWidget *row_widget) {
    const auto match =
        std::ranges::find(rows_, row_widget, &ConditionRow::widget);
    if (match == rows_.end()) {
        return;
    }
    const auto removed_index = static_cast<std::size_t>(match - rows_.begin());
    conditions_layout_->removeWidget(match->widget);
    match->widget->hide();
    match->widget->deleteLater();
    rows_.erase(match);
    if (rows_.empty()) {
        AddCondition();
    } else {
        conditions_layout_->activate();
        auto &focus_row = rows_[std::min(removed_index, rows_.size() - 1)];
        focus_row.field->setFocus(Qt::OtherFocusReason);
        conditions_scroll_->ensureWidgetVisible(focus_row.field, 0, 6);
    }
    UpdateRemoveButtons();
    emit QueryChanged();
}

void TimelineFilterWidget::UpdateConditionEditor(ConditionRow &row) {
    const auto field = static_cast<Field>(row.field->currentData().toInt());
    const auto uses_edit_type = field == Field::kEditType;
    const auto uses_track_kind = field == Field::kTrackKind;
    const auto uses_duration = field == Field::kDuration;
    const auto uses_timecode =
        field == Field::kSourceIn || field == Field::kSourceOut ||
        field == Field::kRecordIn || field == Field::kRecordOut;
    const auto uses_text_options =
        !uses_edit_type && !uses_track_kind && !uses_duration && !uses_timecode;
    if (uses_duration) {
        row.text->setPlaceholderText(tr("Exact duration in frames"));
    } else if (uses_timecode) {
        row.text->setPlaceholderText(tr("Exact timecode (HH:MM:SS:FF)"));
    } else {
        row.text->setPlaceholderText(row.regular_expression->isChecked()
                                         ? tr("Regular expression…")
                                         : tr("Contains text…"));
    }
    row.text->setValidator(uses_duration ? row.duration_validator : nullptr);
    row.text->setVisible(!uses_edit_type && !uses_track_kind);
    row.track_kind->setVisible(uses_track_kind);
    row.edit_type->setVisible(uses_edit_type);
    row.match_case->setVisible(uses_text_options);
    row.match_whole_word->setVisible(uses_text_options);
    row.regular_expression->setVisible(uses_text_options);
}

void TimelineFilterWidget::UpdateRemoveButtons(void) {
    const auto can_remove = rows_.size() > 1;
    for (const auto &row : rows_) {
        row.remove->setEnabled(can_remove);
    }
}

} // namespace edit_atlas::app
