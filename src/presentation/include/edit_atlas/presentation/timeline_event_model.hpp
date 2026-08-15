#ifndef EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_MODEL_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

#include <cstddef>
#include <vector>

namespace edit_atlas::presentation {

/// Presents timeline events lazily for sorting and filtering in the desktop UI.
class TimelineEventModel final : public QAbstractTableModel {
    Q_OBJECT

  public:
    /// Creates an empty model with an optional QObject parent.
    explicit TimelineEventModel(QObject *parent = nullptr);
    /// Destroys the non-owning model.
    ~TimelineEventModel(void) override = default;

    /// Timeline event models are non-copyable QObject owners.
    TimelineEventModel(const TimelineEventModel &) = delete;
    /// Timeline event models are non-copy-assignable QObject owners.
    TimelineEventModel &operator=(const TimelineEventModel &) = delete;
    /// Timeline event models are non-movable QObject owners.
    TimelineEventModel(TimelineEventModel &&) = delete;
    /// Timeline event models are non-move-assignable QObject owners.
    TimelineEventModel &operator=(TimelineEventModel &&) = delete;

    /// Returns the number of selected events for a root model index.
    [[nodiscard]] int
    rowCount(const QModelIndex &parent = QModelIndex{}) const override;
    /// Returns the fixed number of event fields exposed by the model.
    [[nodiscard]] int
    columnCount(const QModelIndex &parent = QModelIndex{}) const override;
    /// Returns localized display or sorting data for one event field.
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    /// Returns localized horizontal field labels and vertical row labels.
    [[nodiscard]] QVariant
    headerData(int section, Qt::Orientation orientation,
               int role = Qt::DisplayRole) const override;

    /// Changes the non-owning document displayed by the model.
    void SetDocument(const core::TimelineDocument *document);
    /// Selects source-document event indices to display in the supplied order.
    void SetEventSelection(std::vector<std::size_t> event_indices);

  private:
    [[nodiscard]] QString EditTypeText(core::EditType edit_type) const;
    [[nodiscard]] QString TrackText(const core::EditEvent &event) const;

    const core::TimelineDocument *document_ = nullptr;
    std::vector<std::size_t> event_indices_;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TIMELINE_EVENT_MODEL_HPP_
