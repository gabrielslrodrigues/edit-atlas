#include <edit_atlas/presentation/timeline_filter_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/services/timeline_filter.hpp>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace edit_atlas::presentation {
namespace {

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] std::string Utf8(const QString &text) {
    const auto value = text.toUtf8();
    return {value.constData(), static_cast<std::size_t>(value.size())};
}

[[nodiscard]] TimelineFilterEditor EditorForField(TimelineFilterField field) {
    switch (field) {
    case TimelineFilterField::kTrackKind:
        return TimelineFilterEditor::kTrackKind;
    case TimelineFilterField::kEditType:
        return TimelineFilterEditor::kEditType;
    case TimelineFilterField::kSourceIn:
    case TimelineFilterField::kSourceOut:
    case TimelineFilterField::kRecordIn:
    case TimelineFilterField::kRecordOut:
        return TimelineFilterEditor::kTimecode;
    case TimelineFilterField::kDuration:
        return TimelineFilterEditor::kDuration;
    case TimelineFilterField::kEventIdentifier:
    case TimelineFilterField::kReel:
    case TimelineFilterField::kTrackIdentifier:
    case TimelineFilterField::kClip:
    case TimelineFilterField::kComments:
        return TimelineFilterEditor::kText;
    case TimelineFilterField::kCount:
        break;
    }
    return TimelineFilterEditor::kText;
}

[[nodiscard]] services::TimelineTextFilterField
TextField(TimelineFilterField field) {
    switch (field) {
    case TimelineFilterField::kEventIdentifier:
        return services::TimelineTextFilterField::kEventIdentifier;
    case TimelineFilterField::kReel:
        return services::TimelineTextFilterField::kReel;
    case TimelineFilterField::kTrackIdentifier:
        return services::TimelineTextFilterField::kTrackIdentifier;
    case TimelineFilterField::kClip:
        return services::TimelineTextFilterField::kClip;
    case TimelineFilterField::kComments:
        return services::TimelineTextFilterField::kComments;
    case TimelineFilterField::kTrackKind:
    case TimelineFilterField::kEditType:
    case TimelineFilterField::kSourceIn:
    case TimelineFilterField::kSourceOut:
    case TimelineFilterField::kRecordIn:
    case TimelineFilterField::kRecordOut:
    case TimelineFilterField::kDuration:
    case TimelineFilterField::kCount:
        break;
    }
    return services::TimelineTextFilterField::kEventIdentifier;
}

[[nodiscard]] services::TimelineTimecodeFilterField
TimecodeField(TimelineFilterField field) {
    switch (field) {
    case TimelineFilterField::kSourceIn:
        return services::TimelineTimecodeFilterField::kSourceIn;
    case TimelineFilterField::kSourceOut:
        return services::TimelineTimecodeFilterField::kSourceOut;
    case TimelineFilterField::kRecordIn:
        return services::TimelineTimecodeFilterField::kRecordIn;
    case TimelineFilterField::kRecordOut:
        return services::TimelineTimecodeFilterField::kRecordOut;
    case TimelineFilterField::kEventIdentifier:
    case TimelineFilterField::kReel:
    case TimelineFilterField::kTrackKind:
    case TimelineFilterField::kTrackIdentifier:
    case TimelineFilterField::kEditType:
    case TimelineFilterField::kClip:
    case TimelineFilterField::kDuration:
    case TimelineFilterField::kComments:
    case TimelineFilterField::kCount:
        break;
    }
    return services::TimelineTimecodeFilterField::kSourceIn;
}

[[nodiscard]] TimelineFilterField
PresentationField(services::TimelineTextFilterField field) {
    switch (field) {
    case services::TimelineTextFilterField::kEventIdentifier:
        return TimelineFilterField::kEventIdentifier;
    case services::TimelineTextFilterField::kReel:
        return TimelineFilterField::kReel;
    case services::TimelineTextFilterField::kTrackIdentifier:
        return TimelineFilterField::kTrackIdentifier;
    case services::TimelineTextFilterField::kClip:
        return TimelineFilterField::kClip;
    case services::TimelineTextFilterField::kComments:
        return TimelineFilterField::kComments;
    }
    return TimelineFilterField::kEventIdentifier;
}

[[nodiscard]] TimelineFilterField
PresentationField(services::TimelineTimecodeFilterField field) {
    switch (field) {
    case services::TimelineTimecodeFilterField::kSourceIn:
        return TimelineFilterField::kSourceIn;
    case services::TimelineTimecodeFilterField::kSourceOut:
        return TimelineFilterField::kSourceOut;
    case services::TimelineTimecodeFilterField::kRecordIn:
        return TimelineFilterField::kRecordIn;
    case services::TimelineTimecodeFilterField::kRecordOut:
        return TimelineFilterField::kRecordOut;
    }
    return TimelineFilterField::kSourceIn;
}

[[nodiscard]] bool IsValidField(int value) {
    return value >= 0 &&
           value < static_cast<int>(TimelineFilterField::kCount);
}

[[nodiscard]] bool IsValidTrackKind(int value) {
    return value >= static_cast<int>(core::TrackKind::kVideo) &&
           value <= static_cast<int>(core::TrackKind::kOther);
}

[[nodiscard]] bool IsValidEditType(int value) {
    return value >= static_cast<int>(core::EditType::kCut) &&
           value <= static_cast<int>(core::EditType::kOther);
}

} // namespace

TimelineFilterModel::TimelineFilterModel(QObject *parent)
    : QAbstractListModel{parent}, rows_(1) {}

int TimelineFilterModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    const auto maximum =
        static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(rows_.size(), maximum));
}

QVariant TimelineFilterModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto &row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case kFieldRole:
        return static_cast<int>(row.field);
    case kEditorRole:
        return static_cast<int>(EditorForField(row.field));
    case kTextRole:
        return row.text;
    case kSelectionRole:
        return row.field == TimelineFilterField::kTrackKind
                   ? static_cast<int>(row.track_kind)
                   : static_cast<int>(row.edit_type);
    case kMatchCaseRole:
        return row.match_case;
    case kMatchWholeWordRole:
        return row.match_whole_word;
    case kRegularExpressionRole:
        return row.regular_expression;
    default:
        return {};
    }
}

bool TimelineFilterModel::setData(const QModelIndex &index,
                                  const QVariant &value, int role) {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return false;
    }
    const auto previous_query = Query();
    auto &row = rows_[static_cast<std::size_t>(index.row())];
    bool changed = false;
    switch (role) {
    case kFieldRole: {
        const auto field = value.toInt();
        if (!IsValidField(field) || static_cast<int>(row.field) == field) {
            return false;
        }
        row.field = static_cast<TimelineFilterField>(field);
        changed = true;
        break;
    }
    case kTextRole: {
        const auto text = value.toString();
        if (row.text == text) {
            return false;
        }
        row.text = text;
        changed = true;
        break;
    }
    case kSelectionRole: {
        const auto selection = value.toInt();
        if (row.field == TimelineFilterField::kTrackKind) {
            if (!IsValidTrackKind(selection) ||
                static_cast<int>(row.track_kind) == selection) {
                return false;
            }
            row.track_kind = static_cast<core::TrackKind>(selection);
            changed = true;
        } else if (row.field == TimelineFilterField::kEditType) {
            if (!IsValidEditType(selection) ||
                static_cast<int>(row.edit_type) == selection) {
                return false;
            }
            row.edit_type = static_cast<core::EditType>(selection);
            changed = true;
        }
        break;
    }
    case kMatchCaseRole: {
        const auto enabled = value.toBool();
        if (row.match_case == enabled) {
            return false;
        }
        row.match_case = enabled;
        changed = true;
        break;
    }
    case kMatchWholeWordRole: {
        const auto enabled = value.toBool();
        if (row.match_whole_word == enabled) {
            return false;
        }
        row.match_whole_word = enabled;
        changed = true;
        break;
    }
    case kRegularExpressionRole: {
        const auto enabled = value.toBool();
        if (row.regular_expression == enabled) {
            return false;
        }
        row.regular_expression = enabled;
        changed = true;
        break;
    }
    default:
        return false;
    }
    if (!changed) {
        return false;
    }
    emit dataChanged(index, index);
    if (Query() != previous_query) {
        emit queryChanged();
    }
    return true;
}

Qt::ItemFlags TimelineFilterModel::flags(const QModelIndex &index) const {
    return index.isValid()
               ? QAbstractListModel::flags(index) | Qt::ItemIsEditable
               : QAbstractListModel::flags(index);
}

QHash<int, QByteArray> TimelineFilterModel::roleNames(void) const {
    return {
        {kFieldRole, "field"},
        {kEditorRole, "editor"},
        {kTextRole, "conditionText"},
        {kSelectionRole, "selection"},
        {kMatchCaseRole, "matchCase"},
        {kMatchWholeWordRole, "matchWholeWord"},
        {kRegularExpressionRole, "regularExpression"},
    };
}

void TimelineFilterModel::AddCondition(void) {
    const auto position = rowCount();
    beginInsertRows({}, position, position);
    rows_.emplace_back();
    endInsertRows();
}

void TimelineFilterModel::RemoveCondition(int row) {
    if (row < 0 || row >= rowCount() || rows_.size() == 1) {
        return;
    }
    const auto previous_query = Query();
    beginRemoveRows({}, row, row);
    rows_.erase(rows_.begin() + static_cast<std::ptrdiff_t>(row));
    endRemoveRows();
    if (Query() != previous_query) {
        emit queryChanged();
    }
}

void TimelineFilterModel::Clear(void) {
    if (combination_ == services::TimelineFilterCombination::kAll &&
        rows_.size() == 1 && rows_.front() == ConditionRow{}) {
        return;
    }
    const auto previous_query = Query();
    beginResetModel();
    combination_ = services::TimelineFilterCombination::kAll;
    rows_.assign(1, ConditionRow{});
    endResetModel();
    if (Query() != previous_query) {
        emit queryChanged();
    }
}

void TimelineFilterModel::SetQuery(
    const services::TimelineFilterQuery &query) {
    const auto previous_query = Query();
    beginResetModel();
    combination_ = query.combination;
    rows_.clear();
    rows_.reserve(std::max<std::size_t>(query.conditions.size(), 1));
    for (const auto &condition : query.conditions) {
        ConditionRow row;
        std::visit(
            [&row](const auto &value) {
                using Condition = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<
                                  Condition,
                                  services::TimelineTextFilterCondition>) {
                    row.field = PresentationField(value.field);
                    row.text = Utf8(value.text);
                    row.match_case = value.match_case;
                    row.match_whole_word = value.match_whole_word;
                    row.regular_expression = value.regular_expression;
                } else if constexpr (std::is_same_v<
                                         Condition,
                                         services::TimelineTrackKindFilterCondition>) {
                    row.field = TimelineFilterField::kTrackKind;
                    row.track_kind = value.track_kind;
                } else if constexpr (std::is_same_v<
                                         Condition,
                                         services::TimelineEditTypeFilterCondition>) {
                    row.field = TimelineFilterField::kEditType;
                    row.edit_type = value.edit_type;
                } else if constexpr (std::is_same_v<
                                         Condition,
                                         services::TimelineTimecodeFilterCondition>) {
                    row.field = PresentationField(value.field);
                    row.text = Utf8(value.timecode);
                } else {
                    row.field = TimelineFilterField::kDuration;
                    row.text = value.frames.has_value()
                                   ? QString::number(
                                         static_cast<qlonglong>(*value.frames))
                                   : QString{};
                }
            },
            condition);
        rows_.emplace_back(std::move(row));
    }
    if (rows_.empty()) {
        rows_.emplace_back();
    }
    endResetModel();
    if (Query() != previous_query) {
        emit queryChanged();
    }
}

services::TimelineFilterQuery TimelineFilterModel::Query(void) const {
    services::TimelineFilterQuery query{
        .combination = combination_,
        .conditions = {},
    };
    query.conditions.reserve(rows_.size());
    for (const auto &row : rows_) {
        switch (EditorForField(row.field)) {
        case TimelineFilterEditor::kTrackKind:
            query.conditions.emplace_back(
                services::TimelineTrackKindFilterCondition{
                    .track_kind = row.track_kind,
                });
            break;
        case TimelineFilterEditor::kEditType:
            query.conditions.emplace_back(
                services::TimelineEditTypeFilterCondition{
                    .edit_type = row.edit_type,
                });
            break;
        case TimelineFilterEditor::kTimecode:
            if (row.text.isEmpty()) {
                break;
            }
            query.conditions.emplace_back(
                services::TimelineTimecodeFilterCondition{
                    .field = TimecodeField(row.field),
                    .timecode = Utf8(row.text),
                });
            break;
        case TimelineFilterEditor::kDuration: {
            bool valid = false;
            const auto frames = row.text.toLongLong(&valid);
            if (!valid || frames < 0) {
                break;
            }
            query.conditions.emplace_back(
                services::TimelineDurationFilterCondition{
                    .frames = std::optional<std::int64_t>{
                        static_cast<std::int64_t>(frames)},
                });
            break;
        }
        case TimelineFilterEditor::kText:
            if (row.text.isEmpty()) {
                break;
            }
            query.conditions.emplace_back(
                services::TimelineTextFilterCondition{
                    .field = TextField(row.field),
                    .text = Utf8(row.text),
                    .match_case = row.match_case,
                    .match_whole_word = row.match_whole_word,
                    .regular_expression = row.regular_expression,
                });
            break;
        }
    }
    return query;
}

int TimelineFilterModel::Combination(void) const noexcept {
    return static_cast<int>(combination_);
}

void TimelineFilterModel::SetCombination(int combination) {
    if (combination < static_cast<int>(
                          services::TimelineFilterCombination::kAll) ||
        combination > static_cast<int>(
                          services::TimelineFilterCombination::kAny) ||
        static_cast<int>(combination_) == combination) {
        return;
    }
    combination_ =
        static_cast<services::TimelineFilterCombination>(combination);
    emit queryChanged();
}

QStringList TimelineFilterModel::CombinationNames(void) const {
    return {tr("All conditions"), tr("Any condition")};
}

QStringList TimelineFilterModel::FieldNames(void) const {
    return {
        tr("Event"),       tr("Reel"),       tr("Track type"),
        tr("Track ID"),    tr("Edit type"),  tr("Clip"),
        tr("Source In"),   tr("Source Out"), tr("Record In"),
        tr("Record Out"),  tr("Duration frames"),
        tr("Comments"),
    };
}

QStringList TimelineFilterModel::TrackKindNames(void) const {
    return {tr("Video"), tr("Audio"), tr("Data"), tr("Other")};
}

QStringList TimelineFilterModel::EditTypeNames(void) const {
    return {tr("Cut"), tr("Dissolve"), tr("Wipe"), tr("Key"), tr("Other")};
}

void TimelineFilterModel::Retranslate(void) { emit displayTextChanged(); }

} // namespace edit_atlas::presentation
