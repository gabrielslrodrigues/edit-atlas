#include <edit_atlas/presentation/timeline_template_model.hpp>

#include <edit_atlas/services/timeline_template.hpp>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <Qt>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace edit_atlas::presentation {
namespace {

[[nodiscard]] QString Utf8(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

TimelineTemplateModel::TimelineTemplateModel(QObject *parent)
    : QAbstractListModel{parent} {}

int TimelineTemplateModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    const auto count = templates_.size() + 1;
    const auto maximum =
        static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(count, maximum));
}

QVariant TimelineTemplateModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const auto no_template = index.row() == 0;
    const auto *value =
        no_template
            ? nullptr
            : &templates_[static_cast<std::size_t>(index.row() - 1)];
    switch (role) {
    case Qt::DisplayRole:
        return no_template ? tr("No template") : Utf8(value->name);
    case kIdentifierRole:
        return no_template ? QString{} : Utf8(value->identifier);
    case kActiveRole:
        return no_template ? active_identifier_.empty()
                           : value->identifier == active_identifier_;
    case kModifiedRole:
        return !no_template && value->identifier == active_identifier_ &&
               modified_;
    default:
        return {};
    }
}

QHash<int, QByteArray> TimelineTemplateModel::roleNames(void) const {
    auto roles = QAbstractListModel::roleNames();
    roles.insert(kIdentifierRole, "identifier");
    roles.insert(kActiveRole, "active");
    roles.insert(kModifiedRole, "modified");
    return roles;
}

void TimelineTemplateModel::SetTemplates(
    std::span<const services::TimelineTemplate> templates,
    std::optional<std::string_view> active_identifier, bool modified) {
    beginResetModel();
    templates_.clear();
    templates_.reserve(templates.size());
    for (const auto &value : templates) {
        templates_.push_back({
            .identifier = value.identifier,
            .name = value.name,
        });
    }
    const auto identifier = active_identifier.value_or(std::string_view{});
    active_identifier_.assign(identifier.data(), identifier.size());
    if (!active_identifier_.empty() &&
        std::ranges::find(templates_, active_identifier_,
                          &TemplateItem::identifier) == templates_.end()) {
        active_identifier_.clear();
    }
    modified_ = modified && !active_identifier_.empty();
    endResetModel();
}

int TimelineTemplateModel::ActiveRow(void) const noexcept {
    if (active_identifier_.empty()) {
        return 0;
    }
    const auto match = std::ranges::find(templates_, active_identifier_,
                                         &TemplateItem::identifier);
    return match == templates_.end()
               ? 0
               : static_cast<int>(std::distance(templates_.begin(), match)) +
                     1;
}

void TimelineTemplateModel::Retranslate(void) {
    emit dataChanged(index(0, 0), index(0, 0), {Qt::DisplayRole});
}

} // namespace edit_atlas::presentation
