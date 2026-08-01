#ifndef EDIT_ATLAS_APP_DOCUMENT_VIEW_HPP_
#define EDIT_ATLAS_APP_DOCUMENT_VIEW_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/services/document_import_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>

#include <QString>
#include <QWidget>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

class QGroupBox;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QTreeWidget;

namespace edit_atlas::app {

class TimelineEventModel;
class TimelineFilterWidget;

/// Presents empty, loading, timeline, and import-failure document states.
class DocumentView final : public QWidget {
    Q_OBJECT

  public:
    explicit DocumentView(QWidget *parent = nullptr);
    ~DocumentView(void) override = default;

    DocumentView(const DocumentView &) = delete;
    DocumentView &operator=(const DocumentView &) = delete;
    DocumentView(DocumentView &&) = delete;
    DocumentView &operator=(DocumentView &&) = delete;

    void Clear(void);
    [[nodiscard]] services::TimelineFilterQuery FilterQuery(void) const;
    void RetranslateUi(void);
    void SetBusy(bool busy);
    void SetEventSelection(std::span<const std::size_t> event_indices);
    void SetFilterError(QString error);
    void ShowDocument(const core::TimelineDocument &document,
                      QString fallback_title,
                      const std::vector<core::Diagnostic> &diagnostics);
    void ShowImportFailure(const services::DocumentImportFailure &failure);
    void ShowLoading(QString file_name);

  signals:
    void ExportRequested(void);
    void FilterChanged(void);
    void OpenRequested(void);

  private:
    void BuildUi(void);
    void PopulateDiagnostics(const std::vector<core::Diagnostic> &diagnostics);
    void RenderDocument(void);
    void RenderImportFailure(void);
    void UpdateControls(void);

    bool busy_ = false;
    bool filter_valid_ = true;
    const core::TimelineDocument *document_ = nullptr;
    QString fallback_title_;
    QString loading_file_name_;
    std::optional<services::DocumentImportFailure> import_failure_;
    std::vector<core::Diagnostic> diagnostics_;
    QLabel *title_label_ = nullptr;
    QLabel *subtitle_label_ = nullptr;
    QStackedWidget *document_stack_ = nullptr;
    QWidget *empty_page_ = nullptr;
    QLabel *empty_title_label_ = nullptr;
    QLabel *empty_description_label_ = nullptr;
    QPushButton *empty_open_button_ = nullptr;
    QWidget *loading_page_ = nullptr;
    QLabel *loading_label_ = nullptr;
    QWidget *timeline_page_ = nullptr;
    QLabel *timeline_title_label_ = nullptr;
    QPushButton *timeline_export_button_ = nullptr;
    QLabel *timeline_summary_label_ = nullptr;
    TimelineFilterWidget *timeline_filter_ = nullptr;
    QLabel *filter_result_label_ = nullptr;
    QTableView *event_table_ = nullptr;
    TimelineEventModel *event_model_ = nullptr;
    QSortFilterProxyModel *event_proxy_model_ = nullptr;
    QWidget *failure_page_ = nullptr;
    QLabel *failure_title_label_ = nullptr;
    QLabel *failure_description_label_ = nullptr;
    QPushButton *failure_open_button_ = nullptr;
    QGroupBox *diagnostics_group_ = nullptr;
    QTreeWidget *diagnostics_tree_ = nullptr;
    QLabel *privacy_label_ = nullptr;
};

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_DOCUMENT_VIEW_HPP_
