#ifndef EDIT_ATLAS_APP_TIMELINE_DOCUMENT_CONTROLLER_HPP_
#define EDIT_ATLAS_APP_TIMELINE_DOCUMENT_CONTROLLER_HPP_

#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>

#include <QObject>
#include <QString>

#include <optional>
#include <string>

class QWidget;

namespace edit_atlas::app {

class ApplicationMenuBar;
class TimelineDocumentView;
class TimelineDocumentWorkflow;
class TimelineTemplateController;

/// Coordinates desktop document import and export interactions.
class TimelineDocumentController final : public QObject {
    Q_OBJECT

  public:
    TimelineDocumentController(const core::FormatRegistry &registry,
                               ApplicationMenuBar &menu_bar,
                               TimelineDocumentView &view,
                               ApplicationLanguage language, QWidget &window);
    ~TimelineDocumentController(void) override = default;

    TimelineDocumentController(const TimelineDocumentController &) = delete;
    TimelineDocumentController &
    operator=(const TimelineDocumentController &) = delete;
    TimelineDocumentController(TimelineDocumentController &&) = delete;
    TimelineDocumentController &
    operator=(TimelineDocumentController &&) = delete;

    void ExportSpreadsheet(void);
    [[nodiscard]] bool IsBusy(void) const noexcept;
    void OpenTimeline(void);
    void OpenTimeline(const QString &path);
    void SetInteractionsEnabled(bool enabled);
    void SetLanguage(ApplicationLanguage language);

  signals:
    void BusyChanged(bool busy);
    void StatusMessageChanged(const QString &message);
    void StatusMessageCleared(void);

  private:
    void ApplyFilter(void);
    void ClearTimeline(void);
    void HandleExportFinished(void);
    void HandleImportFinished(void);
    void
    ShowExportFailure(const services::TimelineDocumentExportFailure &failure);
    void StartImport(const QString &path,
                     std::optional<std::string> frame_rate = std::nullopt);

    const core::FormatRegistry &registry_;
    ApplicationMenuBar &menu_bar_;
    TimelineDocumentView &view_;
    ApplicationLanguage language_;
    QWidget &window_;
    TimelineDocumentWorkflow *workflow_ = nullptr;
    TimelineTemplateController *template_controller_ = nullptr;
    bool interactions_enabled_ = true;
    bool filter_valid_ = true;
    std::optional<core::TimelineDocument> timeline_;
    services::TimelineFilterQuery filter_query_;
    services::TimelineEventSelection event_selection_;
    QString current_path_;
    std::optional<std::string> requested_frame_rate_;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_TIMELINE_DOCUMENT_CONTROLLER_HPP_
