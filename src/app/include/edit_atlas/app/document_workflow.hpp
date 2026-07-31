#ifndef EDIT_ATLAS_APP_DOCUMENT_WORKFLOW_HPP_
#define EDIT_ATLAS_APP_DOCUMENT_WORKFLOW_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/document_export_service.hpp>
#include <edit_atlas/services/document_import_service.hpp>

#include <QFutureWatcher>
#include <QObject>

namespace edit_atlas::app {

/// Owns Qt asynchronous execution for the UI-independent document services.
class DocumentWorkflow final : public QObject {
    Q_OBJECT

  public:
    explicit DocumentWorkflow(const core::FormatRegistry &registry,
                              QObject *parent = nullptr);
    ~DocumentWorkflow(void) override;

    DocumentWorkflow(const DocumentWorkflow &) = delete;
    DocumentWorkflow &operator=(const DocumentWorkflow &) = delete;
    DocumentWorkflow(DocumentWorkflow &&) = delete;
    DocumentWorkflow &operator=(DocumentWorkflow &&) = delete;

    void Export(services::ExportDocumentRequest request);
    [[nodiscard]] services::ExportDocumentResult ExportResult(void) const;
    void Import(services::ImportDocumentRequest request);
    [[nodiscard]] services::ImportDocumentResult ImportResult(void) const;
    [[nodiscard]] bool IsBusy(void) const noexcept;
    [[nodiscard]] bool IsExporting(void) const noexcept;

  signals:
    void ExportFinished(void);
    void ImportFinished(void);

  private:
    services::DocumentExportService export_service_;
    services::DocumentImportService import_service_;
    QFutureWatcher<services::ExportDocumentResult> export_watcher_;
    QFutureWatcher<services::ImportDocumentResult> import_watcher_;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_DOCUMENT_WORKFLOW_HPP_
