#include <edit_atlas/app/timeline_event_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <QAbstractTableModel>
#include <QChar>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <variant>

namespace edit_atlas::app {
namespace {

enum Column {
    kEvent,
    kReel,
    kTrack,
    kEdit,
    kClip,
    kSourceIn,
    kSourceOut,
    kRecordIn,
    kRecordOut,
    kDuration,
    kComments,
    kColumnCount,
};

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QString FormatTimecode(const core::Timecode &timecode) {
    const auto separator =
        timecode.mode() == core::TimecodeMode::kDropFrame ? u';' : u':';
    return QStringLiteral("%1:%2:%3%4%5")
        .arg(timecode.hours(), 2, 10, QChar{u'0'})
        .arg(timecode.minutes(), 2, 10, QChar{u'0'})
        .arg(timecode.seconds(), 2, 10, QChar{u'0'})
        .arg(separator)
        .arg(timecode.frames(), 2, 10, QChar{u'0'});
}

[[nodiscard]] QString MetadataText(const core::EditEvent &event,
                                   std::string_view key) {
    const auto match =
        std::ranges::find(event.metadata, key, &core::MetadataEntry::key);
    if (match == event.metadata.end()) {
        return {};
    }
    const auto *text = std::get_if<std::string>(&match->value);
    return text == nullptr ? QString{} : Utf8(*text);
}

[[nodiscard]] QString CommentsText(const core::EditEvent &event) {
    QStringList comments;
    comments.reserve(static_cast<qsizetype>(event.comments.size()));
    for (const auto &comment : event.comments) {
        comments.emplace_back(Utf8(comment.text));
    }
    return comments.join(QStringLiteral(" · "));
}

} // namespace

TimelineEventModel::TimelineEventModel(QObject *parent)
    : QAbstractTableModel(parent) {}

int TimelineEventModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || document_ == nullptr) {
        return 0;
    }
    const auto maximum =
        static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(document_->events.size(), maximum));
}

int TimelineEventModel::columnCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant TimelineEventModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || document_ == nullptr || index.row() >= rowCount() ||
        index.column() < 0 || index.column() >= kColumnCount) {
        return {};
    }

    const auto &event =
        document_->events[static_cast<std::size_t>(index.row())];
    if (role == Qt::ToolTipRole && event.provenance.has_value()) {
        return tr("Source line %1")
            .arg(static_cast<qulonglong>(event.provenance->location.line));
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    switch (index.column()) {
    case kEvent:
        return Utf8(event.identifier);
    case kReel:
        return Utf8(event.reel);
    case kTrack:
        return TrackText(event);
    case kEdit:
        return EditTypeText(event.edit_type);
    case kClip:
        return MetadataText(event, "clip_name");
    case kSourceIn:
        return FormatTimecode(event.source_range.start());
    case kSourceOut:
        return FormatTimecode(event.source_range.end_exclusive());
    case kRecordIn:
        return FormatTimecode(event.record_range.start());
    case kRecordOut:
        return FormatTimecode(event.record_range.end_exclusive());
    case kDuration:
        return QVariant::fromValue(
            static_cast<qlonglong>(event.record_range.DurationInFrames()));
    case kComments:
        return CommentsText(event);
    default:
        return {};
    }
}

QVariant TimelineEventModel::headerData(int section,
                                        Qt::Orientation orientation,
                                        int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }
    switch (section) {
    case kEvent:
        return tr("Event");
    case kReel:
        return tr("Reel");
    case kTrack:
        return tr("Track");
    case kEdit:
        return tr("Edit");
    case kClip:
        return tr("Clip");
    case kSourceIn:
        return tr("Source In");
    case kSourceOut:
        return tr("Source Out");
    case kRecordIn:
        return tr("Record In");
    case kRecordOut:
        return tr("Record Out");
    case kDuration:
        return tr("Duration");
    case kComments:
        return tr("Comments");
    default:
        return {};
    }
}

QString TimelineEventModel::EditTypeText(core::EditType edit_type) const {
    switch (edit_type) {
    case core::EditType::kCut:
        return tr("Cut");
    case core::EditType::kDissolve:
        return tr("Dissolve");
    case core::EditType::kWipe:
        return tr("Wipe");
    case core::EditType::kKey:
        return tr("Key");
    case core::EditType::kOther:
        return tr("Other");
    }
    return {};
}

QString TimelineEventModel::TrackText(const core::EditEvent &event) const {
    QString track;
    switch (event.track.kind) {
    case core::TrackKind::kVideo:
        track = tr("Video");
        break;
    case core::TrackKind::kAudio:
        track = tr("Audio");
        break;
    case core::TrackKind::kData:
        track = tr("Data");
        break;
    case core::TrackKind::kOther:
        track = tr("Other");
        break;
    }
    return event.track.identifier.empty()
               ? track
               : tr("%1 (%2)").arg(track, Utf8(event.track.identifier));
}

void TimelineEventModel::SetDocument(const core::TimelineDocument *document) {
    beginResetModel();
    document_ = document;
    endResetModel();
}

} // namespace edit_atlas::app
