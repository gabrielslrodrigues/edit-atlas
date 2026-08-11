#ifndef EDIT_ATLAS_APP_EVENT_PROJECTION_WIDGET_HPP_
#define EDIT_ATLAS_APP_EVENT_PROJECTION_WIDGET_HPP_

#include <edit_atlas/core/timeline_projection.hpp>

#include <QWidget>

#include <span>
#include <vector>

class QLabel;
class QListWidget;
class QPushButton;

namespace edit_atlas::app {

/// Edits an ordered, non-empty timeline event projection.
class EventProjectionWidget final : public QWidget {
    Q_OBJECT

  public:
    explicit EventProjectionWidget(
        std::span<const core::TimelineEventField> projection,
        QWidget *parent = nullptr);
    ~EventProjectionWidget(void) override = default;

    [[nodiscard]] std::vector<core::TimelineEventField> Projection(void) const;

    EventProjectionWidget(const EventProjectionWidget &) = delete;
    EventProjectionWidget &operator=(const EventProjectionWidget &) = delete;
    EventProjectionWidget(EventProjectionWidget &&) = delete;
    EventProjectionWidget &operator=(EventProjectionWidget &&) = delete;

  signals:
    /// Emitted whenever selection or ordering changes.
    void ProjectionChanged(void);
    void ValidityChanged(bool valid);

  private:
    void MoveCurrentColumn(int offset);
    void UpdateControls(void);

    QListWidget *columns_ = nullptr;
    QPushButton *move_up_ = nullptr;
    QPushButton *move_down_ = nullptr;
    QLabel *error_ = nullptr;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_EVENT_PROJECTION_WIDGET_HPP_
