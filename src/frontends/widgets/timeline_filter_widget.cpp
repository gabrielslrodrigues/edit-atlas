#include "timeline_filter_widget.hpp"

#include "accessibility.hpp"

#include <QAbstractScrollArea>
#include <QAction>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
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
#include <type_traits>
#include <utility>

namespace edit_atlas::frontends::widgets {
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
    AddCondition(false);
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
    AddCondition(true);
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

void TimelineFilterWidget::SetQuery(
    const services::TimelineFilterQuery &query) {
    {
        const QSignalBlocker blocker{combination_};
        combination_->setCurrentIndex(
            combination_->findData(EnumData(query.combination)));
    }
    for (const auto &row : rows_) {
        conditions_layout_->removeWidget(row.widget);
        row.widget->hide();
        row.widget->deleteLater();
    }
    rows_.clear();
    for (const auto &condition : query.conditions) {
        AddCondition(false);
        ApplyCondition(rows_.back(), condition);
    }
    if (rows_.empty()) {
        AddCondition(false);
    }
    UpdateRemoveButtons();
    emit QueryChanged();
}

void TimelineFilterWidget::SetError(QString error) {
    error_label_->setText(std::move(error));
    error_label_->setVisible(!error_label_->text().isEmpty());
}

void TimelineFilterWidget::SetTemplates(
    std::span<const services::TimelineTemplate> templates,
    std::optional<std::string_view> active_identifier, bool modified) {
    const QSignalBlocker blocker{template_selector_};
    template_selector_->clear();
    template_selector_->addItem(tr("No template"), QString{});
    for (const auto &value : templates) {
        template_selector_->addItem(
            QString::fromUtf8(value.name.data(),
                              static_cast<qsizetype>(value.name.size())),
            QString::fromUtf8(value.identifier.data(),
                              static_cast<qsizetype>(value.identifier.size())));
    }
    auto active_index = 0;
    if (active_identifier.has_value()) {
        const auto identifier = QString::fromUtf8(
            active_identifier->data(),
            static_cast<qsizetype>(active_identifier->size()));
        const auto match = template_selector_->findData(identifier);
        active_index = match < 0 ? 0 : match;
    }
    template_selector_->setCurrentIndex(active_index);
    has_active_template_ = active_index > 0;
    template_modified_ = modified;
    UpdateTemplateControls();
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
    filter_title_label_->setText(tr("Filters"));
    template_label_->setText(tr("Template"));
    template_selector_->setAccessibleName(tr("Template"));
    if (template_selector_->count() > 0) {
        template_selector_->setItemText(0, tr("No template"));
    }
    save_template_action_->setText(tr("Save as…"));
    edit_columns_action_->setText(tr("Export columns…"));
    template_actions_button_->setText(QStringLiteral("⋯"));
    template_actions_button_->setToolTip(tr("Template actions"));
    template_actions_button_->setAccessibleName(tr("Template actions"));
    rename_template_action_->setText(tr("Rename…"));
    duplicate_template_action_->setText(tr("Duplicate…"));
    delete_template_action_->setText(tr("Delete…"));
    UpdateTemplateControls();

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

void TimelineFilterWidget::AddCondition(bool focus) {
    auto *widget = new QWidget{conditions_container_};
    widget->setObjectName(QStringLiteral("filterConditionRow"));
    auto *layout = new QHBoxLayout{widget};
    layout->setContentsMargins(8, 6, 8, 6);
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
    UpdateAccessibleIdentifiers();

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
    if (focus) {
        rows_.back().field->setFocus(Qt::OtherFocusReason);
        conditions_scroll_->ensureWidgetVisible(rows_.back().field, 0, 6);
    }
}

void TimelineFilterWidget::ApplyCondition(
    ConditionRow &row, const services::TimelineFilterCondition &condition) {
    const QSignalBlocker field_blocker{row.field};
    const QSignalBlocker text_blocker{row.text};
    const QSignalBlocker track_kind_blocker{row.track_kind};
    const QSignalBlocker edit_type_blocker{row.edit_type};
    const QSignalBlocker match_case_blocker{row.match_case};
    const QSignalBlocker whole_word_blocker{row.match_whole_word};
    const QSignalBlocker expression_blocker{row.regular_expression};
    std::visit(
        [&](const auto &value) {
            using Condition = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<
                              Condition,
                              services::TimelineTextFilterCondition>) {
                const auto field = [&] {
                    switch (value.field) {
                    case services::TimelineTextFilterField::kEventIdentifier:
                        return Field::kEventIdentifier;
                    case services::TimelineTextFilterField::kReel:
                        return Field::kReel;
                    case services::TimelineTextFilterField::kTrackIdentifier:
                        return Field::kTrackIdentifier;
                    case services::TimelineTextFilterField::kClip:
                        return Field::kClip;
                    case services::TimelineTextFilterField::kComments:
                        return Field::kComments;
                    }
                    std::unreachable();
                }();
                row.field->setCurrentIndex(
                    row.field->findData(EnumData(field)));
                row.text->setText(QString::fromUtf8(
                    value.text.data(),
                    static_cast<qsizetype>(value.text.size())));
                row.match_case->setChecked(value.match_case);
                row.match_whole_word->setChecked(value.match_whole_word);
                row.regular_expression->setChecked(value.regular_expression);
            } else if constexpr (
                std::is_same_v<Condition,
                               services::TimelineTrackKindFilterCondition>) {
                row.field->setCurrentIndex(
                    row.field->findData(EnumData(Field::kTrackKind)));
                row.track_kind->setCurrentIndex(
                    row.track_kind->findData(EnumData(value.track_kind)));
            } else if constexpr (
                std::is_same_v<Condition,
                               services::TimelineEditTypeFilterCondition>) {
                row.field->setCurrentIndex(
                    row.field->findData(EnumData(Field::kEditType)));
                row.edit_type->setCurrentIndex(
                    row.edit_type->findData(EnumData(value.edit_type)));
            } else if constexpr (
                std::is_same_v<Condition,
                               services::TimelineTimecodeFilterCondition>) {
                const auto field = [&] {
                    switch (value.field) {
                    case services::TimelineTimecodeFilterField::kSourceIn:
                        return Field::kSourceIn;
                    case services::TimelineTimecodeFilterField::kSourceOut:
                        return Field::kSourceOut;
                    case services::TimelineTimecodeFilterField::kRecordIn:
                        return Field::kRecordIn;
                    case services::TimelineTimecodeFilterField::kRecordOut:
                        return Field::kRecordOut;
                    }
                    std::unreachable();
                }();
                row.field->setCurrentIndex(
                    row.field->findData(EnumData(field)));
                row.text->setText(QString::fromUtf8(
                    value.timecode.data(),
                    static_cast<qsizetype>(value.timecode.size())));
            } else {
                row.field->setCurrentIndex(
                    row.field->findData(EnumData(Field::kDuration)));
                row.text->setText(value.frames.has_value()
                                      ? QString::number(*value.frames)
                                      : QString{});
            }
        },
        condition);
    UpdateConditionEditor(row);
}

void TimelineFilterWidget::BuildUi(void) {
    auto *root_layout = new QVBoxLayout{this};
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(10);

    auto *template_panel = new QFrame{this};
    template_panel->setObjectName(QStringLiteral("templatePanel"));
    auto *template_layout = new QHBoxLayout{template_panel};
    template_layout->setContentsMargins(10, 8, 10, 8);
    template_layout->setSpacing(8);
    template_label_ = new QLabel{template_panel};
    template_layout->addWidget(template_label_);
    template_selector_ = new QComboBox{template_panel};
    template_layout->addWidget(template_selector_, 1);
    template_status_ = new QLabel{template_panel};
    template_status_->setObjectName(QStringLiteral("templateStatusLabel"));
    template_status_->setVisible(false);
    template_layout->addWidget(template_status_);
    template_primary_button_ = new QPushButton{template_panel};
    template_layout->addWidget(template_primary_button_);
    template_actions_button_ = new QToolButton{template_panel};
    template_actions_button_->setPopupMode(QToolButton::InstantPopup);
    auto *template_menu = new QMenu{template_actions_button_};
    save_template_action_ = template_menu->addAction(QString{});
    edit_columns_action_ = template_menu->addAction(QString{});
    template_menu->addSeparator();
    rename_template_action_ = template_menu->addAction(QString{});
    duplicate_template_action_ = template_menu->addAction(QString{});
    template_menu->addSeparator();
    delete_template_action_ = template_menu->addAction(QString{});
    template_actions_button_->setMenu(template_menu);
    template_layout->addWidget(template_actions_button_);
    root_layout->addWidget(template_panel);

    auto *filter_panel = new QFrame{this};
    filter_panel->setObjectName(QStringLiteral("filterPanel"));
    auto *filter_layout = new QVBoxLayout{filter_panel};
    filter_layout->setContentsMargins(10, 10, 10, 10);
    filter_layout->setSpacing(8);
    auto *toolbar_layout = new QHBoxLayout;
    filter_title_label_ = new QLabel{filter_panel};
    filter_title_label_->setObjectName(QStringLiteral("filterSectionTitle"));
    toolbar_layout->addWidget(filter_title_label_);
    toolbar_layout->addStretch(1);
    combination_label_ = new QLabel{filter_panel};
    toolbar_layout->addWidget(combination_label_);
    combination_ = new QComboBox{filter_panel};
    toolbar_layout->addWidget(combination_);
    add_condition_button_ = new QPushButton{filter_panel};
    toolbar_layout->addWidget(add_condition_button_);
    clear_button_ = new QPushButton{filter_panel};
    toolbar_layout->addWidget(clear_button_);
    filter_layout->addLayout(toolbar_layout);

    auto *divider = new QFrame{filter_panel};
    divider->setObjectName(QStringLiteral("filterDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    filter_layout->addWidget(divider);

    conditions_scroll_ = new QScrollArea{filter_panel};
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
    filter_layout->addWidget(conditions_scroll_, 1);

    error_label_ = new QLabel{filter_panel};
    error_label_->setTextFormat(Qt::PlainText);
    error_label_->setWordWrap(true);
    error_label_->setVisible(false);
    filter_layout->addWidget(error_label_);
    root_layout->addWidget(filter_panel, 1);

    SetAutomationIdentifier(*template_selector_, u"templateSelector");
    SetAutomationIdentifier(*template_primary_button_,
                            u"templatePrimaryButton");
    SetAutomationIdentifier(*template_actions_button_,
                            u"templateActionsButton");
    SetAutomationIdentifier(*template_menu, u"templateActionsMenu");
    SetAutomationIdentifier(*save_template_action_, u"saveTemplateAction");
    SetAutomationIdentifier(*edit_columns_action_, u"editExportColumnsAction");
    SetAutomationIdentifier(*rename_template_action_, u"renameTemplateAction");
    SetAutomationIdentifier(*duplicate_template_action_,
                            u"duplicateTemplateAction");
    SetAutomationIdentifier(*delete_template_action_, u"deleteTemplateAction");
    SetAutomationIdentifier(*combination_, u"filterCombination");
    SetAutomationIdentifier(*add_condition_button_,
                            u"addFilterConditionButton");
    SetAutomationIdentifier(*clear_button_, u"clearFiltersButton");
    SetAutomationIdentifier(*conditions_scroll_, u"filterConditionsScrollArea");
    SetAutomationIdentifier(*error_label_, u"filterErrorLabel");

    connect(combination_, &QComboBox::currentIndexChanged, this,
            [this](int) { emit QueryChanged(); });
    connect(add_condition_button_, &QPushButton::clicked, this,
            [this](void) { AddCondition(true); });
    connect(clear_button_, &QPushButton::clicked, this,
            &TimelineFilterWidget::Clear);
    connect(
        template_selector_, &QComboBox::currentIndexChanged, this, [this](int) {
            emit TemplateSelected(template_selector_->currentData().toString());
        });
    connect(template_primary_button_, &QPushButton::clicked, this,
            [this](void) {
                if (has_active_template_ && template_modified_) {
                    emit UpdateTemplateRequested();
                } else {
                    emit SaveTemplateRequested();
                }
            });
    connect(save_template_action_, &QAction::triggered, this,
            &TimelineFilterWidget::SaveTemplateRequested);
    connect(edit_columns_action_, &QAction::triggered, this,
            &TimelineFilterWidget::EditColumnsRequested);
    connect(rename_template_action_, &QAction::triggered, this,
            &TimelineFilterWidget::RenameTemplateRequested);
    connect(duplicate_template_action_, &QAction::triggered, this,
            &TimelineFilterWidget::DuplicateTemplateRequested);
    connect(delete_template_action_, &QAction::triggered, this,
            &TimelineFilterWidget::DeleteTemplateRequested);
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
    row.field->addItem(tr("Duration frames"), EnumData(Field::kDuration));
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
        AddCondition(true);
    } else {
        conditions_layout_->activate();
        auto &focus_row = rows_[std::min(removed_index, rows_.size() - 1)];
        focus_row.field->setFocus(Qt::OtherFocusReason);
        conditions_scroll_->ensureWidgetVisible(focus_row.field, 0, 6);
    }
    UpdateAccessibleIdentifiers();
    UpdateRemoveButtons();
    emit QueryChanged();
}

void TimelineFilterWidget::UpdateAccessibleIdentifiers(void) {
    for (std::size_t index = 0; index < rows_.size(); ++index) {
        auto &row = rows_[index];
        const auto prefix = QStringLiteral("filterCondition%1")
                                .arg(static_cast<qulonglong>(index));
        SetAccessibilityIdentifier(*row.widget, prefix);
        SetAutomationIdentifier(*row.field, prefix + QStringLiteral("Field"));
        SetAutomationIdentifier(*row.text, prefix + QStringLiteral("Text"));
        SetAutomationIdentifier(*row.track_kind,
                                prefix + QStringLiteral("TrackKind"));
        SetAutomationIdentifier(*row.edit_type,
                                prefix + QStringLiteral("EditType"));
        SetAccessibilityIdentifier(*row.match_case,
                                   prefix + QStringLiteral("MatchCase"));
        SetAccessibilityIdentifier(*row.match_whole_word,
                                   prefix + QStringLiteral("MatchWholeWord"));
        SetAccessibilityIdentifier(*row.regular_expression,
                                   prefix +
                                       QStringLiteral("RegularExpression"));
        SetAccessibilityIdentifier(*row.remove,
                                   prefix + QStringLiteral("Remove"));
    }
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

void TimelineFilterWidget::UpdateTemplateControls(void) {
    template_primary_button_->setText(has_active_template_ && template_modified_
                                          ? tr("Update")
                                          : tr("Save as…"));
    template_status_->setText(template_modified_ ? tr("Modified") : QString{});
    template_status_->setVisible(template_modified_);
    rename_template_action_->setEnabled(has_active_template_);
    duplicate_template_action_->setEnabled(has_active_template_);
    delete_template_action_->setEnabled(has_active_template_);
}

} // namespace edit_atlas::frontends::widgets
