#ifndef EDIT_ATLAS_APP_TIMELINE_EVENT_MODEL_HPP_
#define EDIT_ATLAS_APP_TIMELINE_EVENT_MODEL_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

namespace edit_atlas::app {

/// Presents timeline events lazily for sorting and filtering in the desktop UI.
class TimelineEventModel final : public QAbstractTableModel {
    Q_OBJECT

  public:
    explicit TimelineEventModel(QObject *parent = nullptr);
    ~TimelineEventModel(void) override = default;

    TimelineEventModel(const TimelineEventModel &) = delete;
    TimelineEventModel &operator=(const TimelineEventModel &) = delete;
    TimelineEventModel(TimelineEventModel &&) = delete;
    TimelineEventModel &operator=(TimelineEventModel &&) = delete;

    [[nodiscard]] int
    rowCount(const QModelIndex &parent = QModelIndex{}) const override;
    [[nodiscard]] int
    columnCount(const QModelIndex &parent = QModelIndex{}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant
    headerData(int section, Qt::Orientation orientation,
               int role = Qt::DisplayRole) const override;

    /// Changes the non-owning document displayed by the model.
    void SetDocument(const core::TimelineDocument *document);

  private:
    [[nodiscard]] QString EditTypeText(core::EditType edit_type) const;
    [[nodiscard]] QString TrackText(const core::EditEvent &event) const;

    const core::TimelineDocument *document_ = nullptr;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_TIMELINE_EVENT_MODEL_HPP_
