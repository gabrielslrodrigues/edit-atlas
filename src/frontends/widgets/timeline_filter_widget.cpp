#include "timeline_filter_widget.hpp"

#include "accessibility.hpp"

#include <edit_atlas/presentation/timeline_filter_model.hpp>

#include <QAbstractItemModel>
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
#include <QModelIndex>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>
#include <Qt>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace edit_atlas::frontends::widgets {

TimelineFilterWidget::TimelineFilterWidget(QWidget *parent) : QWidget{parent} {
    BuildUi();
    RetranslateUi();
}

void TimelineFilterWidget::SetModel(presentation::TimelineFilterModel &model) {
    if (model_ == &model) {
        return;
    }
    if (model_ != nullptr) {
        disconnect(model_, nullptr, this, nullptr);
    }
    model_ = &model;
    connect(model_, &presentation::TimelineFilterModel::QueryChanged, this,
            [this](void) {
                const QSignalBlocker blocker{combination_};
                combination_->setCurrentIndex(
                    combination_->findData(model_->Combination()));
                emit QueryChanged();
            });
    connect(model_, &presentation::TimelineFilterModel::DisplayTextChanged,
            this, [this](void) {
                SynchronizeCombination();
                for (auto &row : rows_) {
                    PopulateFields(row);
                    PopulateTrackKinds(row);
                    PopulateEditTypes(row);
                }
            });
    connect(model_, &QAbstractItemModel::modelReset, this,
            [this](void) { RebuildConditionRows(); });
    connect(model_, &QAbstractItemModel::rowsInserted, this,
            [this](const QModelIndex &, int first, int last) {
                for (auto row = first; row <= last; ++row) {
                    AddConditionRow(row);
                }
                UpdateAccessibleIdentifiers();
                UpdateRemoveButtons();
            });
    connect(model_, &QAbstractItemModel::rowsAboutToBeRemoved, this,
            [this](const QModelIndex &, int first, int last) {
                for (auto row = last; row >= first; --row) {
                    auto &condition = rows_[static_cast<std::size_t>(row)];
                    conditions_layout_->removeWidget(condition.widget);
                    condition.widget->hide();
                    condition.widget->deleteLater();
                    rows_.erase(rows_.begin() +
                                static_cast<std::ptrdiff_t>(row));
                }
            });
    connect(model_, &QAbstractItemModel::rowsRemoved, this,
            [this](const QModelIndex &, int first, int) {
                UpdateAccessibleIdentifiers();
                UpdateRemoveButtons();
                if (!rows_.empty()) {
                    conditions_layout_->activate();
                    const auto focus_row =
                        std::min(first, static_cast<int>(rows_.size() - 1));
                    auto *field =
                        rows_[static_cast<std::size_t>(focus_row)].field;
                    field->setFocus(Qt::OtherFocusReason);
                    conditions_scroll_->ensureWidgetVisible(field, 0, 6);
                }
            });
    connect(model_, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &first, const QModelIndex &last) {
                for (auto row = first.row(); row <= last.row(); ++row) {
                    SynchronizeConditionRow(
                        rows_[static_cast<std::size_t>(row)], row);
                }
            });
    RebuildConditionRows();
}

void TimelineFilterWidget::Clear(void) {
    SetError({});
    if (model_ == nullptr) {
        return;
    }
    model_->Clear();
    if (!rows_.empty()) {
        rows_.front().field->setFocus(Qt::OtherFocusReason);
    }
}

services::TimelineFilterQuery TimelineFilterWidget::Query(void) const {
    return model_ == nullptr ? services::TimelineFilterQuery{}
                             : model_->Query();
}

void TimelineFilterWidget::SetQuery(
    const services::TimelineFilterQuery &query) {
    if (model_ != nullptr) {
        model_->SetQuery(query);
    }
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
    SynchronizeCombination();
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
        const auto model_row = static_cast<int>(&row - rows_.data());
        UpdateConditionEditor(row, model_row);
    }
}

void TimelineFilterWidget::AddCondition(bool focus) {
    if (model_ == nullptr) {
        return;
    }
    const auto row = model_->rowCount();
    model_->AddCondition();
    if (focus && row < static_cast<int>(rows_.size())) {
        rows_[static_cast<std::size_t>(row)].field->setFocus(
            Qt::OtherFocusReason);
        conditions_scroll_->ensureWidgetVisible(
            rows_[static_cast<std::size_t>(row)].field, 0, 6);
    }
}

void TimelineFilterWidget::AddConditionRow(int model_row) {
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
    conditions_layout_->insertWidget(model_row, widget);
    rows_.insert(rows_.begin() + static_cast<std::ptrdiff_t>(model_row), row);

    connect(row.field, &QComboBox::currentIndexChanged, this,
            [this, widget, field = row.field](int) {
                SetConditionData(widget, field->currentData(),
                                 presentation::TimelineFilterModel::kFieldRole);
            });
    connect(row.text, &QLineEdit::textChanged, this,
            [this, widget](const QString &text) {
                SetConditionData(widget, text,
                                 presentation::TimelineFilterModel::kTextRole);
            });
    connect(row.edit_type, &QComboBox::currentIndexChanged, this,
            [this, widget, edit_type = row.edit_type](int) {
                SetConditionData(
                    widget, edit_type->currentData(),
                    presentation::TimelineFilterModel::kSelectionRole);
            });
    connect(row.track_kind, &QComboBox::currentIndexChanged, this,
            [this, widget, track_kind = row.track_kind](int) {
                SetConditionData(
                    widget, track_kind->currentData(),
                    presentation::TimelineFilterModel::kSelectionRole);
            });
    connect(row.match_case, &QToolButton::toggled, this,
            [this, widget](bool checked) {
                SetConditionData(
                    widget, checked,
                    presentation::TimelineFilterModel::kMatchCaseRole);
            });
    connect(row.match_whole_word, &QToolButton::toggled, this,
            [this, widget](bool checked) {
                SetConditionData(
                    widget, checked,
                    presentation::TimelineFilterModel::kMatchWholeWordRole);
            });
    connect(row.regular_expression, &QToolButton::toggled, this,
            [this, widget](bool checked) {
                SetConditionData(
                    widget, checked,
                    presentation::TimelineFilterModel::kRegularExpressionRole);
            });
    connect(row.remove, &QPushButton::clicked, this,
            [this, widget](void) { RemoveCondition(widget); });

    auto &added = rows_[static_cast<std::size_t>(model_row)];
    PopulateFields(added);
    PopulateTrackKinds(added);
    PopulateEditTypes(added);
    added.match_case->setToolTip(tr("Match case"));
    added.match_case->setAccessibleName(tr("Match case"));
    added.match_whole_word->setToolTip(tr("Match whole word"));
    added.match_whole_word->setAccessibleName(tr("Match whole word"));
    added.regular_expression->setToolTip(tr("Use regular expression"));
    added.regular_expression->setAccessibleName(tr("Use regular expression"));
    added.remove->setText(tr("Remove"));
    SynchronizeConditionRow(added, model_row);
    conditions_layout_->activate();
    if (rows_.size() == 1) {
        conditions_scroll_->setMinimumHeight(widget->sizeHint().height());
    }
}

void TimelineFilterWidget::ClearConditionRows(void) {
    for (const auto &row : rows_) {
        conditions_layout_->removeWidget(row.widget);
        row.widget->hide();
        row.widget->deleteLater();
    }
    rows_.clear();
}

void TimelineFilterWidget::RebuildConditionRows(void) {
    ClearConditionRows();
    if (model_ == nullptr) {
        return;
    }
    SynchronizeCombination();
    for (auto row = 0; row < model_->rowCount(); ++row) {
        AddConditionRow(row);
    }
    UpdateAccessibleIdentifiers();
    UpdateRemoveButtons();
}

void TimelineFilterWidget::SynchronizeConditionRow(ConditionRow &row,
                                                   int model_row) {
    if (model_ == nullptr) {
        return;
    }
    const auto index = model_->index(model_row, 0);
    const QSignalBlocker field_blocker{row.field};
    const QSignalBlocker text_blocker{row.text};
    const QSignalBlocker track_kind_blocker{row.track_kind};
    const QSignalBlocker edit_type_blocker{row.edit_type};
    const QSignalBlocker match_case_blocker{row.match_case};
    const QSignalBlocker whole_word_blocker{row.match_whole_word};
    const QSignalBlocker expression_blocker{row.regular_expression};
    row.field->setCurrentIndex(row.field->findData(
        model_->data(index, presentation::TimelineFilterModel::kFieldRole)));
    row.text->setText(
        model_->data(index, presentation::TimelineFilterModel::kTextRole)
            .toString());
    const auto selection =
        model_->data(index, presentation::TimelineFilterModel::kSelectionRole);
    row.track_kind->setCurrentIndex(row.track_kind->findData(selection));
    row.edit_type->setCurrentIndex(row.edit_type->findData(selection));
    row.match_case->setChecked(
        model_->data(index, presentation::TimelineFilterModel::kMatchCaseRole)
            .toBool());
    row.match_whole_word->setChecked(
        model_
            ->data(index,
                   presentation::TimelineFilterModel::kMatchWholeWordRole)
            .toBool());
    row.regular_expression->setChecked(
        model_
            ->data(index,
                   presentation::TimelineFilterModel::kRegularExpressionRole)
            .toBool());
    UpdateConditionEditor(row, model_row);
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

    connect(combination_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (model_ != nullptr) {
            model_->SetCombination(combination_->currentData().toInt());
        }
    });
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
    if (model_ != nullptr) {
        const auto names = model_->EditTypeNames();
        for (qsizetype index = 0; index < names.size(); ++index) {
            row.edit_type->addItem(names[index], static_cast<int>(index));
        }
    }
    const auto index = row.edit_type->findData(edit_type);
    row.edit_type->setCurrentIndex(index < 0 ? 0 : index);
}

void TimelineFilterWidget::PopulateFields(ConditionRow &row) {
    const auto field = row.field->currentData();
    const QSignalBlocker blocker{row.field};
    row.field->clear();
    if (model_ != nullptr) {
        const auto names = model_->FieldNames();
        for (qsizetype index = 0; index < names.size(); ++index) {
            row.field->addItem(names[index], static_cast<int>(index));
        }
    }
    const auto index = row.field->findData(field);
    row.field->setCurrentIndex(index < 0 ? 0 : index);
}

void TimelineFilterWidget::PopulateTrackKinds(ConditionRow &row) {
    const auto track_kind = row.track_kind->currentData();
    const QSignalBlocker blocker{row.track_kind};
    row.track_kind->clear();
    if (model_ != nullptr) {
        const auto names = model_->TrackKindNames();
        for (qsizetype index = 0; index < names.size(); ++index) {
            row.track_kind->addItem(names[index], static_cast<int>(index));
        }
    }
    const auto index = row.track_kind->findData(track_kind);
    row.track_kind->setCurrentIndex(index < 0 ? 0 : index);
}

void TimelineFilterWidget::RemoveCondition(QWidget *row_widget) {
    const auto match =
        std::ranges::find(rows_, row_widget, &ConditionRow::widget);
    if (match == rows_.end() || model_ == nullptr) {
        return;
    }
    model_->RemoveCondition(static_cast<int>(match - rows_.begin()));
}

void TimelineFilterWidget::SetConditionData(QWidget *row_widget,
                                            const QVariant &value, int role) {
    if (model_ == nullptr) {
        return;
    }
    const auto match =
        std::ranges::find(rows_, row_widget, &ConditionRow::widget);
    if (match == rows_.end()) {
        return;
    }
    const auto model_row = static_cast<int>(match - rows_.begin());
    static_cast<void>(
        model_->setData(model_->index(model_row, 0), value, role));
}

void TimelineFilterWidget::SynchronizeCombination(void) {
    if (model_ == nullptr) {
        return;
    }
    const QSignalBlocker blocker{combination_};
    combination_->clear();
    const auto names = model_->CombinationNames();
    for (qsizetype index = 0; index < names.size(); ++index) {
        combination_->addItem(names[index], static_cast<int>(index));
    }
    combination_->setCurrentIndex(
        combination_->findData(model_->Combination()));
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

void TimelineFilterWidget::UpdateConditionEditor(ConditionRow &row,
                                                 int model_row) {
    if (model_ == nullptr) {
        return;
    }
    const auto editor = static_cast<presentation::TimelineFilterEditor>(
        model_
            ->data(model_->index(model_row, 0),
                   presentation::TimelineFilterModel::kEditorRole)
            .toInt());
    const auto uses_edit_type =
        editor == presentation::TimelineFilterEditor::kEditType;
    const auto uses_track_kind =
        editor == presentation::TimelineFilterEditor::kTrackKind;
    const auto uses_duration =
        editor == presentation::TimelineFilterEditor::kDuration;
    const auto uses_timecode =
        editor == presentation::TimelineFilterEditor::kTimecode;
    const auto uses_text_options =
        editor == presentation::TimelineFilterEditor::kText;
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
