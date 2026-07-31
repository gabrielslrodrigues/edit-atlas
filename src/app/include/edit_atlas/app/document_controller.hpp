#ifndef EDIT_ATLAS_APP_DOCUMENT_CONTROLLER_HPP_
#define EDIT_ATLAS_APP_DOCUMENT_CONTROLLER_HPP_

#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/document_export_service.hpp>
#include <edit_atlas/services/document_import_service.hpp>

#include <QObject>
#include <QString>

#include <optional>
#include <string>

class QWidget;

namespace edit_atlas::app {

class ApplicationMenuBar;
class DocumentView;
class DocumentWorkflow;

/// Coordinates desktop document import and export interactions.
class DocumentController final : public QObject {
    Q_OBJECT

  public:
    DocumentController(const core::FormatRegistry &registry,
                       ApplicationMenuBar &menu_bar, DocumentView &view,
                       ApplicationLanguage language, QWidget &window);
    ~DocumentController(void) override = default;

    DocumentController(const DocumentController &) = delete;
    DocumentController &operator=(const DocumentController &) = delete;
    DocumentController(DocumentController &&) = delete;
    DocumentController &operator=(DocumentController &&) = delete;

    void ExportSpreadsheet(void);
    [[nodiscard]] bool IsBusy(void) const noexcept;
    void OpenDocument(void);
    void OpenDocument(const QString &path);
    void SetInteractionsEnabled(bool enabled);
    void SetLanguage(ApplicationLanguage language);

  signals:
    void BusyChanged(bool busy);
    void StatusMessageChanged(const QString &message);
    void StatusMessageCleared(void);

  private:
    void ClearDocument(void);
    void HandleExportFinished(void);
    void HandleImportFinished(void);
    void ShowExportFailure(const services::DocumentExportFailure &failure);
    void StartImport(const QString &path,
                     std::optional<std::string> frame_rate = std::nullopt);

    const core::FormatRegistry &registry_;
    ApplicationMenuBar &menu_bar_;
    DocumentView &view_;
    ApplicationLanguage language_;
    QWidget &window_;
    DocumentWorkflow *workflow_ = nullptr;
    bool interactions_enabled_ = true;
    std::optional<core::TimelineDocument> document_;
    QString current_path_;
    std::optional<std::string> requested_frame_rate_;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_DOCUMENT_CONTROLLER_HPP_
