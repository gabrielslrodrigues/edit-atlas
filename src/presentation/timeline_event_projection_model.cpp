#include <edit_atlas/presentation/timeline_event_projection_model.hpp>

#include <edit_atlas/core/timeline_projection.hpp>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <Qt>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace edit_atlas::presentation {
namespace {

[[nodiscard]] QString FieldText(core::TimelineEventField field) {
    switch (field) {
    case core::TimelineEventField::kEventIdentifier:
        return TimelineEventProjectionModel::tr("Event");
    case core::TimelineEventField::kInitialFrame:
        return TimelineEventProjectionModel::tr("Initial frame");
    case core::TimelineEventField::kReel:
        return TimelineEventProjectionModel::tr("Reel");
    case core::TimelineEventField::kTrackKind:
        return TimelineEventProjectionModel::tr("Track type");
    case core::TimelineEventField::kTrackIdentifier:
        return TimelineEventProjectionModel::tr("Track");
    case core::TimelineEventField::kEditType:
        return TimelineEventProjectionModel::tr("Edit type");
    case core::TimelineEventField::kTransitionIdentifier:
        return TimelineEventProjectionModel::tr("Transition");
    case core::TimelineEventField::kTransitionDuration:
        return TimelineEventProjectionModel::tr("Transition frames");
    case core::TimelineEventField::kSourceIn:
        return TimelineEventProjectionModel::tr("Source in");
    case core::TimelineEventField::kSourceOut:
        return TimelineEventProjectionModel::tr("Source out");
    case core::TimelineEventField::kRecordIn:
        return TimelineEventProjectionModel::tr("Record in");
    case core::TimelineEventField::kRecordOut:
        return TimelineEventProjectionModel::tr("Record out");
    case core::TimelineEventField::kDuration:
        return TimelineEventProjectionModel::tr("Duration");
    case core::TimelineEventField::kDurationFrames:
        return TimelineEventProjectionModel::tr("Duration frames");
    case core::TimelineEventField::kClipName:
        return TimelineEventProjectionModel::tr("Clip name");
    case core::TimelineEventField::kSourceFile:
        return TimelineEventProjectionModel::tr("Source file");
    case core::TimelineEventField::kComments:
        return TimelineEventProjectionModel::tr("Comments");
    case core::TimelineEventField::kSourceLine:
        return TimelineEventProjectionModel::tr("Source line");
    case core::TimelineEventField::kCount:
        break;
    }
    return {};
}

[[nodiscard]] bool IsKnownField(core::TimelineEventField field) {
    return field >= core::TimelineEventField::kEventIdentifier &&
           field < core::TimelineEventField::kCount;
}

[[nodiscard]] std::optional<std::array<bool, core::kTimelineEventFieldCount>>
SelectedFields(std::span<const core::TimelineEventField> projection) {
    std::array<bool, core::kTimelineEventFieldCount> selected{};
    for (const auto field : projection) {
        if (!IsKnownField(field)) {
            return std::nullopt;
        }
        const auto index = static_cast<std::size_t>(field);
        if (selected[index]) {
            return std::nullopt;
        }
        selected[index] = true;
    }
    return selected;
}

} // namespace

TimelineEventProjectionModel::TimelineEventProjectionModel(QObject *parent)
    : QAbstractListModel{parent} {
    static_cast<void>(SetProjection(core::DefaultTimelineEventProjection()));
}

int TimelineEventProjectionModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    const auto maximum =
        static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(rows_.size(), maximum));
}

QVariant TimelineEventProjectionModel::data(const QModelIndex &index,
                                            int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto &row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case Qt::DisplayRole:
        return FieldText(row.field);
    case kIdentifierRole: {
        const auto identifier = core::TimelineEventFieldIdentifier(row.field);
        return QString::fromUtf8(identifier.data(),
                                 static_cast<qsizetype>(identifier.size()));
    }
    case kFieldRole:
        return static_cast<int>(row.field);
    case kSelectedRole:
        return row.selected;
    default:
        return {};
    }
}

bool TimelineEventProjectionModel::setData(const QModelIndex &index,
                                           const QVariant &value, int role) {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() ||
        role != kSelectedRole) {
        return false;
    }
    auto &row = rows_[static_cast<std::size_t>(index.row())];
    const auto selected = value.toBool();
    if (row.selected == selected) {
        return false;
    }
    row.selected = selected;
    emit dataChanged(index, index, {kSelectedRole});
    emit ProjectionChanged();
    return true;
}

Qt::ItemFlags
TimelineEventProjectionModel::flags(const QModelIndex &index) const {
    return index.isValid()
               ? QAbstractListModel::flags(index) | Qt::ItemIsEditable
               : QAbstractListModel::flags(index);
}

QHash<int, QByteArray> TimelineEventProjectionModel::roleNames(void) const {
    auto roles = QAbstractListModel::roleNames();
    roles.insert(kIdentifierRole, "identifier");
    roles.insert(kFieldRole, "field");
    roles.insert(kSelectedRole, "selected");
    return roles;
}

void TimelineEventProjectionModel::SetSelected(int row, bool selected) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    static_cast<void>(setData(index(row, 0), selected, kSelectedRole));
}

void TimelineEventProjectionModel::MoveUp(int row) { Move(row, -1); }

void TimelineEventProjectionModel::MoveDown(int row) { Move(row, 1); }

bool TimelineEventProjectionModel::SetProjection(
    std::span<const core::TimelineEventField> projection) {
    const auto selected = SelectedFields(projection);
    if (!selected.has_value()) {
        return false;
    }
    beginResetModel();
    rows_.clear();
    rows_.reserve(core::kTimelineEventFieldCount);
    for (const auto field : projection) {
        rows_.push_back({.field = field, .selected = true});
    }
    for (const auto field : core::TimelineEventFields()) {
        if (!(*selected)[static_cast<std::size_t>(field)]) {
            rows_.push_back({.field = field, .selected = false});
        }
    }
    endResetModel();
    emit ProjectionChanged();
    return true;
}

std::vector<core::TimelineEventField>
TimelineEventProjectionModel::Projection(void) const {
    std::vector<core::TimelineEventField> projection;
    projection.reserve(rows_.size());
    for (const auto &row : rows_) {
        if (row.selected) {
            projection.push_back(row.field);
        }
    }
    return projection;
}

int TimelineEventProjectionModel::SelectedCount(void) const noexcept {
    return static_cast<int>(
        std::ranges::count(rows_, true, &FieldRow::selected));
}

bool TimelineEventProjectionModel::IsValid(void) const noexcept {
    return SelectedCount() != 0;
}

void TimelineEventProjectionModel::Retranslate(void) {
    if (!rows_.empty()) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0),
                         {Qt::DisplayRole});
    }
}

void TimelineEventProjectionModel::Move(int row, int offset) {
    const auto target = row + offset;
    if (row < 0 || row >= rowCount() || target < 0 || target >= rowCount()) {
        return;
    }
    const auto source = static_cast<std::size_t>(row);
    const auto destination = static_cast<std::size_t>(target);
    beginMoveRows({}, row, row, {}, target > row ? target + 1 : target);
    if (destination < source) {
        std::rotate(rows_.begin() + static_cast<std::ptrdiff_t>(destination),
                    rows_.begin() + static_cast<std::ptrdiff_t>(source),
                    rows_.begin() + static_cast<std::ptrdiff_t>(source + 1));
    } else {
        std::rotate(rows_.begin() + static_cast<std::ptrdiff_t>(source),
                    rows_.begin() + static_cast<std::ptrdiff_t>(source + 1),
                    rows_.begin() +
                        static_cast<std::ptrdiff_t>(destination + 1));
    }
    endMoveRows();
    emit ProjectionChanged();
}

} // namespace edit_atlas::presentation
