#ifndef EDIT_ATLAS_APP_EVENT_PROJECTION_DIALOG_HPP_
#define EDIT_ATLAS_APP_EVENT_PROJECTION_DIALOG_HPP_

#include <edit_atlas/core/timeline_projection.hpp>

#include <QDialog>

#include <span>
#include <vector>

class QPushButton;
class QWidget;

namespace edit_atlas::app {

class EventProjectionWidget;

/// Collects a valid event-column projection for templates and exports.
class EventProjectionDialog final : public QDialog {
    Q_OBJECT

  public:
    explicit EventProjectionDialog(
        std::span<const core::TimelineEventField> projection,
        QWidget *parent = nullptr);
    ~EventProjectionDialog(void) override = default;

    [[nodiscard]] std::vector<core::TimelineEventField> Projection(void) const;

    EventProjectionDialog(const EventProjectionDialog &) = delete;
    EventProjectionDialog &operator=(const EventProjectionDialog &) = delete;
    EventProjectionDialog(EventProjectionDialog &&) = delete;
    EventProjectionDialog &operator=(EventProjectionDialog &&) = delete;

  private:
    EventProjectionWidget *projection_ = nullptr;
    QPushButton *save_ = nullptr;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_EVENT_PROJECTION_DIALOG_HPP_
