#ifndef EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_PROJECTION_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_PROJECTION_MODEL_HPP_

#include <edit_atlas/core/timeline_projection.hpp>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QObject>
#include <QVariant>
#include <Qt>

#include <span>
#include <vector>

namespace edit_atlas::presentation {

/// Presents an editable ordered selection of timeline export fields.
class TimelineEventProjectionModel final : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int selectedCount READ SelectedCount NOTIFY ProjectionChanged)
    Q_PROPERTY(bool valid READ IsValid NOTIFY ProjectionChanged)

  public:
    /// Roles describing one available timeline event field.
    enum Role {
        /// Stable, nonlocalized field identifier.
        kIdentifierRole = Qt::UserRole,
        /// Stable `TimelineEventField` integer value.
        kFieldRole,
        /// Whether the field is included in the projection.
        kSelectedRole,
    };
    Q_ENUM(Role)

    /// Creates a model using the default event projection.
    explicit TimelineEventProjectionModel(QObject *parent = nullptr);
    /// Destroys the ordered field rows.
    ~TimelineEventProjectionModel(void) override = default;

    TimelineEventProjectionModel(const TimelineEventProjectionModel &) = delete;
    TimelineEventProjectionModel &
    operator=(const TimelineEventProjectionModel &) = delete;
    TimelineEventProjectionModel(TimelineEventProjectionModel &&) = delete;
    TimelineEventProjectionModel &
    operator=(TimelineEventProjectionModel &&) = delete;

    /// Returns every available timeline event field.
    [[nodiscard]] int
    rowCount(const QModelIndex &parent = QModelIndex{}) const override;
    /// Returns localized display text and stable field state.
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    /// Updates whether one field is selected.
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    /// Returns editable list-item flags for valid field rows.
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
    /// Returns stable QML role names for field state.
    [[nodiscard]] QHash<int, QByteArray> roleNames(void) const override;

    /// Selects or excludes one field row.
    Q_INVOKABLE void SetSelected(int row, bool selected);
    /// Moves one field row upward when possible.
    Q_INVOKABLE void MoveUp(int row);
    /// Moves one field row downward when possible.
    Q_INVOKABLE void MoveDown(int row);

    /// Replaces field ordering and selection from an ordered projection.
    [[nodiscard]] bool
    SetProjection(std::span<const core::TimelineEventField> projection);
    /// Returns selected fields in their displayed order.
    [[nodiscard]] std::vector<core::TimelineEventField> Projection(void) const;
    /// Returns the number of selected event fields.
    [[nodiscard]] int SelectedCount(void) const noexcept;
    /// Returns whether at least one field is selected.
    [[nodiscard]] bool IsValid(void) const noexcept;
    /// Notifies views that localized field text changed.
    void Retranslate(void);

  signals:
    /// Reports that selection or ordering changed.
    void ProjectionChanged(void);

  private:
    struct FieldRow final {
        core::TimelineEventField field;
        bool selected;
    };

    void Move(int row, int offset);

    std::vector<FieldRow> rows_;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_PROJECTION_MODEL_HPP_
