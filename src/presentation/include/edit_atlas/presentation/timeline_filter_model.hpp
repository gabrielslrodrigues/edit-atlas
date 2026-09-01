#ifndef EDIT_ATLAS_PRESENTATION_TIMELINE_FILTER_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_TIMELINE_FILTER_MODEL_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/services/timeline_filter.hpp>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>

#include <vector>

namespace edit_atlas::presentation {

/// Identifies one user-selectable timeline filter field.
enum class TimelineFilterField {
    /// Source event identifier.
    kEventIdentifier,
    /// Source reel identifier.
    kReel,
    /// Domain track kind.
    kTrackKind,
    /// Source track identifier.
    kTrackIdentifier,
    /// Editorial operation type.
    kEditType,
    /// Imported clip name.
    kClip,
    /// Inclusive source timecode.
    kSourceIn,
    /// Exclusive source timecode.
    kSourceOut,
    /// Inclusive record timecode.
    kRecordIn,
    /// Exclusive record timecode.
    kRecordOut,
    /// Exact record duration in frames.
    kDuration,
    /// Human-authored comments.
    kComments,
    /// Number of user-selectable filter fields.
    kCount,
};

/// Identifies the editor required by a timeline filter field.
enum class TimelineFilterEditor {
    /// Free-text matching controls.
    kText,
    /// Exact track-kind selection.
    kTrackKind,
    /// Exact edit-type selection.
    kEditType,
    /// Exact timecode entry.
    kTimecode,
    /// Exact duration-in-frames entry.
    kDuration,
};

/// Presents an editable timeline filter query as frontend-neutral Qt state.
class TimelineFilterModel final : public QAbstractListModel {
    Q_OBJECT

    /// Whether every condition or any condition must match.
    Q_PROPERTY(int combination READ Combination WRITE SetCombination NOTIFY
                   queryChanged)
    /// Localized names for the available condition-combination modes.
    Q_PROPERTY(QStringList combinationNames READ CombinationNames NOTIFY
                   displayTextChanged)
    /// Localized names for the available filter fields.
    Q_PROPERTY(QStringList fieldNames READ FieldNames NOTIFY displayTextChanged)
    /// Localized names for the available track kinds.
    Q_PROPERTY(QStringList trackKindNames READ TrackKindNames NOTIFY
                   displayTextChanged)
    /// Localized names for the available edit types.
    Q_PROPERTY(QStringList editTypeNames READ EditTypeNames NOTIFY
                   displayTextChanged)

  public:
    /// Roles used to edit one filter-condition row.
    enum Role {
        /// Selected `TimelineFilterField` integer value.
        kFieldRole = Qt::UserRole,
        /// Read-only `TimelineFilterEditor` integer value.
        kEditorRole,
        /// Text, timecode, or duration editor contents.
        kTextRole,
        /// Selected `TrackKind` or `EditType` integer value.
        kSelectionRole,
        /// Whether text matching is case-sensitive.
        kMatchCaseRole,
        /// Whether text matching requires complete words.
        kMatchWholeWordRole,
        /// Whether text is interpreted as an RE2 expression.
        kRegularExpressionRole,
    };
    Q_ENUM(Role)

    /// Creates a filter model containing one empty text condition.
    explicit TimelineFilterModel(QObject *parent = nullptr);
    /// Destroys the model and its editable condition state.
    ~TimelineFilterModel(void) override = default;

    TimelineFilterModel(const TimelineFilterModel &) = delete;
    TimelineFilterModel &operator=(const TimelineFilterModel &) = delete;
    TimelineFilterModel(TimelineFilterModel &&) = delete;
    TimelineFilterModel &operator=(TimelineFilterModel &&) = delete;

    /// Returns the number of editable condition rows.
    [[nodiscard]] int
    rowCount(const QModelIndex &parent = QModelIndex{}) const override;
    /// Returns one editable condition value for the requested role.
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    /// Updates one editable condition value.
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    /// Returns editable list-item flags for valid condition rows.
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
    /// Returns stable QML role names for condition values.
    [[nodiscard]] QHash<int, QByteArray> roleNames(void) const override;

    /// Appends one empty text condition.
    Q_INVOKABLE void AddCondition(void);
    /// Removes a condition while retaining at least one editable row.
    Q_INVOKABLE void RemoveCondition(int row);
    /// Restores an all-conditions query containing one empty row.
    Q_INVOKABLE void Clear(void);

    /// Replaces the editable state with a presentation-independent query.
    void SetQuery(const services::TimelineFilterQuery &query);
    /// Returns the presentation-independent query represented by the model.
    [[nodiscard]] services::TimelineFilterQuery Query(void) const;
    /// Returns the selected condition-combination value.
    [[nodiscard]] int Combination(void) const noexcept;
    /// Selects how non-empty conditions are combined.
    void SetCombination(int combination);

    /// Returns localized condition-combination names in stable value order.
    [[nodiscard]] QStringList CombinationNames(void) const;
    /// Returns localized filter-field names in stable value order.
    [[nodiscard]] QStringList FieldNames(void) const;
    /// Returns localized track-kind names in stable value order.
    [[nodiscard]] QStringList TrackKindNames(void) const;
    /// Returns localized edit-type names in stable value order.
    [[nodiscard]] QStringList EditTypeNames(void) const;
    /// Notifies consumers that localized choice text changed.
    void Retranslate(void);

  signals:
    /// Reports that the represented filter query changed.
    void queryChanged(void);
    /// Reports that localized choice text changed.
    void displayTextChanged(void);

  private:
    struct ConditionRow final {
        TimelineFilterField field = TimelineFilterField::kEventIdentifier;
        QString text;
        core::TrackKind track_kind = core::TrackKind::kVideo;
        core::EditType edit_type = core::EditType::kCut;
        bool match_case = false;
        bool match_whole_word = false;
        bool regular_expression = false;

        bool operator==(const ConditionRow &) const = default;
    };

    services::TimelineFilterCombination combination_ =
        services::TimelineFilterCombination::kAll;
    std::vector<ConditionRow> rows_;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TIMELINE_FILTER_MODEL_HPP_
