#ifndef EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_DOCUMENT_VIEW_HPP_
#define EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_DOCUMENT_VIEW_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/presentation/timeline_event_model.hpp>
#include <edit_atlas/presentation/timeline_filter_model.hpp>

#include <edit_atlas/services/timeline_document_import_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_template.hpp>

#include <QString>
#include <QWidget>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

class QGroupBox;
class QLabel;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QTableView;
class QTreeWidget;

namespace edit_atlas::frontends::widgets {

class TimelineFilterWidget;

/// Presents empty, loading, timeline, and import-failure document states.
class TimelineDocumentView final : public QWidget {
    Q_OBJECT

  public:
    explicit TimelineDocumentView(QWidget *parent = nullptr);
    ~TimelineDocumentView(void) override = default;

    TimelineDocumentView(const TimelineDocumentView &) = delete;
    TimelineDocumentView &operator=(const TimelineDocumentView &) = delete;
    TimelineDocumentView(TimelineDocumentView &&) = delete;
    TimelineDocumentView &operator=(TimelineDocumentView &&) = delete;

    void Clear(void);
    [[nodiscard]] services::TimelineFilterQuery FilterQuery(void) const;
    void RetranslateUi(void);
    void SetBusy(bool busy);
    void SetEventModel(presentation::TimelineEventModel &event_model);
    /// Attaches shared editable timeline-filter presentation state.
    void SetFilterModel(presentation::TimelineFilterModel &filter_model);
    void SetFilterError(QString error);
    void SetFilterQuery(const services::TimelineFilterQuery &query);
    void SetTemplates(std::span<const services::TimelineTemplate> templates,
                      std::optional<std::string_view> active_identifier,
                      bool modified);
    void SetVisibleEventCount(std::size_t event_count);
    void ShowTimeline(const core::TimelineDocument &document,
                      QString fallback_title,
                      std::span<const core::Diagnostic> diagnostics);
    void
    ShowImportFailure(const services::TimelineDocumentImportFailure &failure);
    void ShowLoading(QString file_name);

  signals:
    void ExportRequested(void);
    void FilterChanged(void);
    void OpenRequested(void);
    void DeleteTemplateRequested(void);
    void DuplicateTemplateRequested(void);
    void EditColumnsRequested(void);
    void RenameTemplateRequested(void);
    void SaveTemplateRequested(void);
    void TemplateSelected(const QString &identifier);
    void UpdateTemplateRequested(void);

  private:
    void BuildUi(void);
    void PopulateDiagnostics(const std::vector<core::Diagnostic> &diagnostics);
    void RenderTimeline(void);
    void RenderImportFailure(void);
    void UpdateControls(void);

    bool busy_ = false;
    bool filter_valid_ = true;
    const core::TimelineDocument *timeline_ = nullptr;
    QString fallback_title_;
    QString loading_file_name_;
    std::optional<services::TimelineDocumentImportFailure> import_failure_;
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
    QSortFilterProxyModel *event_proxy_model_ = nullptr;
    QWidget *failure_page_ = nullptr;
    QLabel *failure_title_label_ = nullptr;
    QLabel *failure_description_label_ = nullptr;
    QPushButton *failure_open_button_ = nullptr;
    QGroupBox *diagnostics_group_ = nullptr;
    QTreeWidget *diagnostics_tree_ = nullptr;
    QLabel *privacy_label_ = nullptr;
};

} // namespace edit_atlas::frontends::widgets

#endif // EDIT_ATLAS_FRONTENDS_WIDGETS_TIMELINE_DOCUMENT_VIEW_HPP_
