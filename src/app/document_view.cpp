#include <edit_atlas/app/document_view.hpp>

#include <edit_atlas/app/diagnostic_text.hpp>
#include <edit_atlas/app/timeline_event_model.hpp>

#include "timeline_filter_widget.hpp"

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <edit_atlas/services/document_import_service.hpp>

#include <QAbstractItemView>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QString>
#include <QTableView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace edit_atlas::app {
namespace {

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

DocumentView::DocumentView(QWidget *parent) : QWidget{parent} {
    setObjectName(QStringLiteral("applicationShell"));
    BuildUi();
    RetranslateUi();
}

void DocumentView::Clear(void) {
    document_ = nullptr;
    fallback_title_.clear();
    loading_file_name_.clear();
    import_failure_.reset();
    diagnostics_.clear();
    event_model_->SetDocument(nullptr);
    const QSignalBlocker filter_blocker{timeline_filter_};
    timeline_filter_->Clear();
    filter_valid_ = true;
    filter_result_label_->clear();
    diagnostics_tree_->clear();
    diagnostics_group_->setVisible(false);
    document_stack_->setCurrentWidget(empty_page_);
    UpdateControls();
}

void DocumentView::SetFilterError(QString error) {
    filter_valid_ = error.isEmpty();
    timeline_filter_->SetError(std::move(error));
    UpdateControls();
}

services::TimelineFilterQuery DocumentView::FilterQuery(void) const {
    return timeline_filter_->Query();
}

void DocumentView::RetranslateUi(void) {
    title_label_->setText(tr("Edit Atlas"));
    subtitle_label_->setText(tr("Private tools for editorial timelines."));
    empty_title_label_->setText(tr("No timeline open"));
    empty_description_label_->setText(
        tr("Open or drop a CMX 3600 EDL to inspect its edit events."));
    empty_open_button_->setText(tr("Open Timeline…"));
    loading_label_->setAccessibleName(tr("Opening timeline"));
    timeline_filter_->RetranslateUi();
    event_table_->setAccessibleName(tr("Timeline edit events"));
    diagnostics_tree_->setAccessibleName(tr("Import diagnostics"));
    timeline_export_button_->setText(tr("Export Spreadsheet"));
    timeline_export_button_->setToolTip(
        tr("Export the currently shown timeline events as an Excel workbook."));
    privacy_label_->setText(
        tr("Your media and timeline data stay on this computer."));
    failure_open_button_->setText(tr("Open Another Timeline…"));

    if (!loading_file_name_.isEmpty()) {
        loading_label_->setText(tr("Opening %1…").arg(loading_file_name_));
    }
    if (document_ != nullptr) {
        RenderDocument();
        PopulateDiagnostics(diagnostics_);
    } else if (import_failure_.has_value()) {
        RenderImportFailure();
        PopulateDiagnostics(diagnostics_);
    }
}

void DocumentView::SetEventSelection(
    std::span<const std::size_t> event_indices) {
    event_model_->SetEventSelection(
        std::vector<std::size_t>{event_indices.begin(), event_indices.end()});
    const auto total =
        document_ == nullptr ? std::size_t{0} : document_->events.size();
    filter_result_label_->setText(
        tr("Showing %1 of %2 events")
            .arg(static_cast<qulonglong>(event_indices.size()))
            .arg(static_cast<qulonglong>(total)));
}

void DocumentView::SetBusy(bool busy) {
    busy_ = busy;
    UpdateControls();
}

void DocumentView::ShowDocument(
    const core::TimelineDocument &document, QString fallback_title,
    const std::vector<core::Diagnostic> &diagnostics) {
    document_ = &document;
    fallback_title_ = std::move(fallback_title);
    loading_file_name_.clear();
    import_failure_.reset();
    diagnostics_ = diagnostics;
    RenderDocument();
    PopulateDiagnostics(diagnostics_);
    document_stack_->setCurrentWidget(timeline_page_);
    UpdateControls();
}

void DocumentView::ShowImportFailure(
    const services::DocumentImportFailure &failure) {
    document_ = nullptr;
    fallback_title_.clear();
    loading_file_name_.clear();
    import_failure_ = failure;
    diagnostics_ = failure.diagnostics;
    event_model_->SetDocument(nullptr);
    RenderImportFailure();
    PopulateDiagnostics(diagnostics_);
    document_stack_->setCurrentWidget(failure_page_);
    UpdateControls();
}

void DocumentView::ShowLoading(QString file_name) {
    loading_file_name_ = std::move(file_name);
    loading_label_->setText(tr("Opening %1…").arg(loading_file_name_));
    document_stack_->setCurrentWidget(loading_page_);
}

void DocumentView::BuildUi(void) {
    auto *root_layout = new QVBoxLayout{this};
    root_layout->setContentsMargins(24, 18, 24, 18);
    root_layout->setSpacing(8);

    title_label_ = new QLabel{this};
    title_label_->setObjectName(QStringLiteral("titleLabel"));
    auto title_font = title_label_->font();
    title_font.setPointSize(18);
    title_font.setWeight(QFont::DemiBold);
    title_label_->setFont(title_font);
    root_layout->addWidget(title_label_);

    subtitle_label_ = new QLabel{this};
    subtitle_label_->setObjectName(QStringLiteral("subtitleLabel"));
    subtitle_label_->setWordWrap(true);
    root_layout->addWidget(subtitle_label_);

    document_stack_ = new QStackedWidget{this};
    document_stack_->setObjectName(QStringLiteral("documentStack"));
    root_layout->addWidget(document_stack_, 1);

    empty_page_ = new QWidget{document_stack_};
    auto *empty_layout = new QVBoxLayout{empty_page_};
    empty_layout->addStretch(1);
    empty_title_label_ = new QLabel{empty_page_};
    empty_title_label_->setAlignment(Qt::AlignCenter);
    auto empty_title_font = empty_title_label_->font();
    empty_title_font.setPointSize(16);
    empty_title_font.setWeight(QFont::DemiBold);
    empty_title_label_->setFont(empty_title_font);
    empty_layout->addWidget(empty_title_label_);
    empty_description_label_ = new QLabel{empty_page_};
    empty_description_label_->setObjectName(
        QStringLiteral("emptyDescriptionLabel"));
    empty_description_label_->setAlignment(Qt::AlignCenter);
    empty_description_label_->setWordWrap(true);
    empty_layout->addWidget(empty_description_label_);
    empty_open_button_ = new QPushButton{empty_page_};
    empty_open_button_->setObjectName(QStringLiteral("emptyOpenButton"));
    empty_open_button_->setDefault(true);
    connect(empty_open_button_, &QPushButton::clicked, this,
            &DocumentView::OpenRequested);
    auto *empty_button_layout = new QHBoxLayout;
    empty_button_layout->addStretch(1);
    empty_button_layout->addWidget(empty_open_button_);
    empty_button_layout->addStretch(1);
    empty_layout->addLayout(empty_button_layout);
    empty_layout->addStretch(1);
    document_stack_->addWidget(empty_page_);

    loading_page_ = new QWidget{document_stack_};
    auto *loading_layout = new QVBoxLayout{loading_page_};
    loading_layout->addStretch(1);
    loading_label_ = new QLabel{loading_page_};
    loading_label_->setAlignment(Qt::AlignCenter);
    loading_layout->addWidget(loading_label_);
    auto *loading_progress = new QProgressBar{loading_page_};
    loading_progress->setRange(0, 0);
    loading_progress->setTextVisible(false);
    loading_layout->addWidget(loading_progress);
    loading_layout->addStretch(1);
    document_stack_->addWidget(loading_page_);

    timeline_page_ = new QWidget{document_stack_};
    auto *timeline_layout = new QVBoxLayout{timeline_page_};
    timeline_layout->setContentsMargins(0, 4, 0, 0);
    auto *timeline_header_layout = new QHBoxLayout;
    timeline_title_label_ = new QLabel{timeline_page_};
    auto timeline_title_font = timeline_title_label_->font();
    timeline_title_font.setPointSize(14);
    timeline_title_font.setWeight(QFont::DemiBold);
    timeline_title_label_->setFont(timeline_title_font);
    timeline_header_layout->addWidget(timeline_title_label_, 1);
    timeline_export_button_ = new QPushButton{timeline_page_};
    timeline_export_button_->setObjectName(
        QStringLiteral("timelineExportButton"));
    connect(timeline_export_button_, &QPushButton::clicked, this,
            &DocumentView::ExportRequested);
    timeline_header_layout->addWidget(timeline_export_button_);
    timeline_layout->addLayout(timeline_header_layout);
    timeline_summary_label_ = new QLabel{timeline_page_};
    timeline_summary_label_->setObjectName(QStringLiteral("timelineSummary"));
    timeline_layout->addWidget(timeline_summary_label_);
    timeline_filter_ = new TimelineFilterWidget{timeline_page_};
    timeline_filter_->setObjectName(QStringLiteral("timelineFilter"));
    timeline_filter_->setSizePolicy(QSizePolicy::Preferred,
                                    QSizePolicy::Expanding);
    connect(timeline_filter_, &TimelineFilterWidget::QueryChanged, this,
            &DocumentView::FilterChanged);
    timeline_layout->addWidget(timeline_filter_, 1);
    filter_result_label_ = new QLabel{timeline_page_};
    filter_result_label_->setObjectName(QStringLiteral("filterResultLabel"));
    timeline_layout->addWidget(filter_result_label_);
    event_model_ = new TimelineEventModel{this};
    event_proxy_model_ = new QSortFilterProxyModel{this};
    event_proxy_model_->setSourceModel(event_model_);
    event_table_ = new QTableView{timeline_page_};
    event_table_->setObjectName(QStringLiteral("eventTable"));
    event_table_->setModel(event_proxy_model_);
    event_table_->setAlternatingRowColors(true);
    event_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    event_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    event_table_->setSortingEnabled(true);
    event_table_->setWordWrap(false);
    event_table_->verticalHeader()->setVisible(false);
    timeline_layout->addWidget(event_table_, 2);
    document_stack_->addWidget(timeline_page_);

    failure_page_ = new QWidget{document_stack_};
    auto *failure_layout = new QVBoxLayout{failure_page_};
    failure_layout->addStretch(1);
    failure_title_label_ = new QLabel{failure_page_};
    failure_title_label_->setAlignment(Qt::AlignCenter);
    auto failure_font = failure_title_label_->font();
    failure_font.setPointSize(15);
    failure_font.setWeight(QFont::DemiBold);
    failure_title_label_->setFont(failure_font);
    failure_layout->addWidget(failure_title_label_);
    failure_description_label_ = new QLabel{failure_page_};
    failure_description_label_->setAlignment(Qt::AlignCenter);
    failure_description_label_->setWordWrap(true);
    failure_layout->addWidget(failure_description_label_);
    failure_open_button_ = new QPushButton{failure_page_};
    failure_open_button_->setObjectName(QStringLiteral("failureOpenButton"));
    connect(failure_open_button_, &QPushButton::clicked, this,
            &DocumentView::OpenRequested);
    auto *retry_layout = new QHBoxLayout;
    retry_layout->addStretch(1);
    retry_layout->addWidget(failure_open_button_);
    retry_layout->addStretch(1);
    failure_layout->addLayout(retry_layout);
    failure_layout->addStretch(1);
    document_stack_->addWidget(failure_page_);

    diagnostics_group_ = new QGroupBox{this};
    diagnostics_group_->setObjectName(QStringLiteral("diagnosticsGroup"));
    auto *diagnostics_layout = new QVBoxLayout{diagnostics_group_};
    diagnostics_tree_ = new QTreeWidget{diagnostics_group_};
    diagnostics_tree_->setObjectName(QStringLiteral("diagnosticsTree"));
    diagnostics_tree_->setRootIsDecorated(false);
    diagnostics_tree_->setAlternatingRowColors(true);
    diagnostics_layout->addWidget(diagnostics_tree_);
    diagnostics_group_->setVisible(false);
    root_layout->addWidget(diagnostics_group_);

    privacy_label_ = new QLabel{this};
    privacy_label_->setObjectName(QStringLiteral("privacyLabel"));
    privacy_label_->setWordWrap(true);
    root_layout->addWidget(privacy_label_);
}

void DocumentView::PopulateDiagnostics(
    const std::vector<core::Diagnostic> &diagnostics) {
    diagnostics_tree_->clear();
    diagnostics_tree_->setHeaderLabels(
        {tr("Severity"), tr("Line"), tr("Message")});

    for (const auto &diagnostic : diagnostics) {
        QString severity;
        switch (diagnostic.severity) {
        case core::DiagnosticSeverity::kInfo:
            severity = tr("Info");
            break;
        case core::DiagnosticSeverity::kWarning:
            severity = tr("Warning");
            break;
        case core::DiagnosticSeverity::kError:
            severity = tr("Error");
            break;
        }
        const auto line =
            diagnostic.location.has_value() && diagnostic.location->line != 0
                ? QString::number(
                      static_cast<qulonglong>(diagnostic.location->line))
                : QString{};
        auto *item = new QTreeWidgetItem{
            diagnostics_tree_,
            {severity, line, diagnostic_text::Message(diagnostic)},
        };
        item->setToolTip(2, Utf8(diagnostic.code));
    }

    diagnostics_group_->setTitle(
        tr("Diagnostics (%1)")
            .arg(static_cast<qulonglong>(diagnostics.size())));
    diagnostics_group_->setVisible(!diagnostics.empty());
    diagnostics_tree_->resizeColumnToContents(0);
    diagnostics_tree_->resizeColumnToContents(1);
}

void DocumentView::RenderDocument(void) {
    timeline_title_label_->setText(
        document_->title.empty() ? fallback_title_ : Utf8(document_->title));
    const auto rate = document_->frame_rate.denominator() == 1
                          ? QString::number(document_->frame_rate.numerator())
                          : QStringLiteral("%1/%2")
                                .arg(document_->frame_rate.numerator())
                                .arg(document_->frame_rate.denominator());
    timeline_summary_label_->setText(
        tr("%1 events · %2 fps · %3")
            .arg(static_cast<qulonglong>(document_->events.size()))
            .arg(rate)
            .arg(document_->timecode_mode == core::TimecodeMode::kDropFrame
                     ? tr("drop-frame")
                     : tr("non-drop-frame")));

    event_model_->SetDocument(document_);
    filter_result_label_->setText(
        tr("Showing %1 of %2 events")
            .arg(static_cast<qulonglong>(document_->events.size()))
            .arg(static_cast<qulonglong>(document_->events.size())));
    event_table_->resizeColumnsToContents();
}

void DocumentView::RenderImportFailure(void) {
    failure_title_label_->setText(tr("Could not open timeline"));

    if (import_failure_->kind ==
        services::DocumentImportFailureKind::kOpenFailed) {
        failure_description_label_->setText(
            tr("The file could not be opened: %1")
                .arg(Utf8(import_failure_->filesystem_error.message())));
    } else if (import_failure_->kind ==
               services::DocumentImportFailureKind::kReadFailed) {
        failure_description_label_->setText(
            tr("The file could not be read: %1")
                .arg(Utf8(import_failure_->filesystem_error.message())));
    } else {
        failure_description_label_->setText(
            tr("The file is not a supported timeline or contains fatal "
               "errors. Review the diagnostics below."));
    }
}

void DocumentView::UpdateControls(void) {
    empty_open_button_->setEnabled(!busy_);
    failure_open_button_->setEnabled(!busy_);
    timeline_export_button_->setEnabled(!busy_ && document_ != nullptr &&
                                        filter_valid_);
    timeline_filter_->setEnabled(!busy_ && document_ != nullptr);
}

} // namespace edit_atlas::app
