#include <edit_atlas/app/main_window.hpp>

#include <edit_atlas/app/timeline_event_model.hpp>

#include <edit_atlas/core/document_pipeline.hpp>
#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/version.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/services/document_service.hpp>

#include <QAbstractItemView>
#include <QAction>
#include <QByteArray>
#include <QComboBox>
#include <QDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::app {
namespace {

constexpr int kLoadingPage = 1;
constexpr int kTimelinePage = 2;
constexpr int kFailurePage = 3;
constexpr qsizetype kMaximumRecentFiles = 10;

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] std::filesystem::path FilesystemPath(const QString &path) {
    const auto utf8 = path.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

[[nodiscard]] QString PathText(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return QString::fromUtf8(reinterpret_cast<const char *>(utf8.data()),
                             static_cast<qsizetype>(utf8.size()));
}

[[nodiscard]] bool
HasDiagnosticCode(const std::vector<core::Diagnostic> &diagnostics,
                  std::string_view code) {
    return std::ranges::any_of(diagnostics,
                               [code](const core::Diagnostic &diagnostic) {
                                   return diagnostic.code == code;
                               });
}

} // namespace

MainWindow::MainWindow(const core::FormatRegistry &registry,
                       QTranslator &translator,
                       ApplicationLanguage initial_language, QWidget *parent)
    : QMainWindow(parent), registry_(registry), document_service_(registry),
      translator_(translator), language_(initial_language) {
    setObjectName(QStringLiteral("mainWindow"));
    resize(1100, 700);
    setMinimumSize(760, 500);
    setAcceptDrops(true);

    connect(&import_watcher_,
            &QFutureWatcher<services::OpenDocumentResult>::finished, this,
            &MainWindow::HandleImportFinished);

    BuildMenus();
    BuildUi();
    RetranslateUi();
}

MainWindow::~MainWindow(void) {
    if (import_watcher_.isRunning()) {
        import_watcher_.waitForFinished();
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUi();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (!import_watcher_.isRunning() && event->mimeData()->hasUrls() &&
        std::ranges::any_of(event->mimeData()->urls(), [](const QUrl &url) {
            return url.isLocalFile();
        })) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    const auto urls = event->mimeData()->urls();
    const auto local_file = std::ranges::find_if(
        urls, [](const QUrl &url) { return url.isLocalFile(); });
    if (local_file == urls.end()) {
        return;
    }
    event->acceptProposedAction();
    OpenDocument(local_file->toLocalFile());
}

void MainWindow::BuildMenus(void) {
    file_menu_ = menuBar()->addMenu(QString{});

    open_action_ = file_menu_->addAction(QString{});
    open_action_->setObjectName(QStringLiteral("openDocumentAction"));
    open_action_->setShortcut(QKeySequence::Open);
    connect(open_action_, &QAction::triggered, this,
            [this](void) { OpenDocument(); });

    recent_files_menu_ = file_menu_->addMenu(QString{});

    remember_recent_action_ = file_menu_->addAction(QString{});
    remember_recent_action_->setCheckable(true);
    const QSettings settings;
    remember_recent_action_->setChecked(
        settings.value(QStringLiteral("files/rememberRecent"), false).toBool());
    connect(remember_recent_action_, &QAction::toggled, this,
            &MainWindow::SetRememberRecentFiles);

    file_menu_->addSeparator();
    exit_action_ = file_menu_->addAction(QString{});
    exit_action_->setShortcut(QKeySequence::Quit);
    connect(exit_action_, &QAction::triggered, this, &QWidget::close);

    help_menu_ = menuBar()->addMenu(QString{});
    about_action_ = help_menu_->addAction(QString{});
    connect(about_action_, &QAction::triggered, this,
            &MainWindow::ShowAboutDialog);

    language_selector_ = new QComboBox{this};
    language_selector_->setObjectName(QStringLiteral("languageSelector"));
    language_selector_->addItem(
        QStringLiteral("Português (Brasil)"),
        static_cast<int>(ApplicationLanguage::kBrazilianPortuguese));
    language_selector_->addItem(
        QStringLiteral("English"),
        static_cast<int>(ApplicationLanguage::kEnglish));
    language_selector_->setMinimumContentsLength(18);
    connect(language_selector_, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                const auto language = static_cast<ApplicationLanguage>(
                    language_selector_->itemData(index).toInt());
                ChangeLanguage(language);
            });
    menuBar()->setCornerWidget(language_selector_, Qt::TopRightCorner);
}

void MainWindow::BuildUi(void) {
    auto *central_widget = new QWidget{this};
    central_widget->setObjectName(QStringLiteral("applicationShell"));
    setCentralWidget(central_widget);

    auto *root_layout = new QVBoxLayout{central_widget};
    root_layout->setContentsMargins(24, 18, 24, 18);
    root_layout->setSpacing(8);

    title_label_ = new QLabel{central_widget};
    title_label_->setObjectName(QStringLiteral("titleLabel"));
    auto title_font = title_label_->font();
    title_font.setPointSize(18);
    title_font.setWeight(QFont::DemiBold);
    title_label_->setFont(title_font);
    root_layout->addWidget(title_label_);

    subtitle_label_ = new QLabel{central_widget};
    subtitle_label_->setObjectName(QStringLiteral("subtitleLabel"));
    subtitle_label_->setWordWrap(true);
    root_layout->addWidget(subtitle_label_);

    document_stack_ = new QStackedWidget{central_widget};
    document_stack_->setObjectName(QStringLiteral("documentStack"));
    root_layout->addWidget(document_stack_, 1);

    auto *empty_page = new QWidget{document_stack_};
    auto *empty_layout = new QVBoxLayout{empty_page};
    empty_layout->addStretch(1);
    empty_title_label_ = new QLabel{empty_page};
    empty_title_label_->setAlignment(Qt::AlignCenter);
    auto empty_title_font = empty_title_label_->font();
    empty_title_font.setPointSize(16);
    empty_title_font.setWeight(QFont::DemiBold);
    empty_title_label_->setFont(empty_title_font);
    empty_layout->addWidget(empty_title_label_);
    empty_description_label_ = new QLabel{empty_page};
    empty_description_label_->setObjectName(
        QStringLiteral("emptyDescriptionLabel"));
    empty_description_label_->setAlignment(Qt::AlignCenter);
    empty_description_label_->setWordWrap(true);
    empty_layout->addWidget(empty_description_label_);
    empty_open_button_ = new QPushButton{empty_page};
    empty_open_button_->setObjectName(QStringLiteral("emptyOpenButton"));
    empty_open_button_->setDefault(true);
    connect(empty_open_button_, &QPushButton::clicked, this,
            [this](void) { OpenDocument(); });
    auto *empty_button_layout = new QHBoxLayout;
    empty_button_layout->addStretch(1);
    empty_button_layout->addWidget(empty_open_button_);
    empty_button_layout->addStretch(1);
    empty_layout->addLayout(empty_button_layout);
    empty_layout->addStretch(1);
    document_stack_->addWidget(empty_page);

    auto *loading_page = new QWidget{document_stack_};
    auto *loading_layout = new QVBoxLayout{loading_page};
    loading_layout->addStretch(1);
    loading_label_ = new QLabel{loading_page};
    loading_label_->setAlignment(Qt::AlignCenter);
    loading_layout->addWidget(loading_label_);
    auto *loading_progress = new QProgressBar{loading_page};
    loading_progress->setRange(0, 0);
    loading_progress->setTextVisible(false);
    loading_layout->addWidget(loading_progress);
    loading_layout->addStretch(1);
    document_stack_->addWidget(loading_page);

    auto *timeline_page = new QWidget{document_stack_};
    auto *timeline_layout = new QVBoxLayout{timeline_page};
    timeline_layout->setContentsMargins(0, 4, 0, 0);
    timeline_title_label_ = new QLabel{timeline_page};
    auto timeline_title_font = timeline_title_label_->font();
    timeline_title_font.setPointSize(14);
    timeline_title_font.setWeight(QFont::DemiBold);
    timeline_title_label_->setFont(timeline_title_font);
    timeline_layout->addWidget(timeline_title_label_);
    timeline_summary_label_ = new QLabel{timeline_page};
    timeline_summary_label_->setObjectName(QStringLiteral("timelineSummary"));
    timeline_layout->addWidget(timeline_summary_label_);
    event_filter_ = new QLineEdit{timeline_page};
    event_filter_->setObjectName(QStringLiteral("eventFilter"));
    event_filter_->setClearButtonEnabled(true);
    timeline_layout->addWidget(event_filter_);
    event_model_ = new TimelineEventModel{this};
    event_proxy_model_ = new QSortFilterProxyModel{this};
    event_proxy_model_->setSourceModel(event_model_);
    event_proxy_model_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    event_proxy_model_->setFilterKeyColumn(-1);
    connect(event_filter_, &QLineEdit::textChanged, event_proxy_model_,
            &QSortFilterProxyModel::setFilterFixedString);
    event_table_ = new QTableView{timeline_page};
    event_table_->setObjectName(QStringLiteral("eventTable"));
    event_table_->setModel(event_proxy_model_);
    event_table_->setAlternatingRowColors(true);
    event_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    event_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    event_table_->setSortingEnabled(true);
    event_table_->setWordWrap(false);
    event_table_->verticalHeader()->setVisible(false);
    timeline_layout->addWidget(event_table_, 1);
    document_stack_->addWidget(timeline_page);

    auto *failure_page = new QWidget{document_stack_};
    auto *failure_layout = new QVBoxLayout{failure_page};
    failure_layout->addStretch(1);
    failure_title_label_ = new QLabel{failure_page};
    failure_title_label_->setAlignment(Qt::AlignCenter);
    auto failure_font = failure_title_label_->font();
    failure_font.setPointSize(15);
    failure_font.setWeight(QFont::DemiBold);
    failure_title_label_->setFont(failure_font);
    failure_layout->addWidget(failure_title_label_);
    failure_description_label_ = new QLabel{failure_page};
    failure_description_label_->setAlignment(Qt::AlignCenter);
    failure_description_label_->setWordWrap(true);
    failure_layout->addWidget(failure_description_label_);
    failure_open_button_ = new QPushButton{failure_page};
    failure_open_button_->setObjectName(QStringLiteral("failureOpenButton"));
    connect(failure_open_button_, &QPushButton::clicked, this,
            [this](void) { OpenDocument(); });
    auto *retry_layout = new QHBoxLayout;
    retry_layout->addStretch(1);
    retry_layout->addWidget(failure_open_button_);
    retry_layout->addStretch(1);
    failure_layout->addLayout(retry_layout);
    failure_layout->addStretch(1);
    document_stack_->addWidget(failure_page);

    diagnostics_group_ = new QGroupBox{central_widget};
    diagnostics_group_->setObjectName(QStringLiteral("diagnosticsGroup"));
    auto *diagnostics_layout = new QVBoxLayout{diagnostics_group_};
    diagnostics_tree_ = new QTreeWidget{diagnostics_group_};
    diagnostics_tree_->setObjectName(QStringLiteral("diagnosticsTree"));
    diagnostics_tree_->setRootIsDecorated(false);
    diagnostics_tree_->setAlternatingRowColors(true);
    diagnostics_layout->addWidget(diagnostics_tree_);
    diagnostics_group_->setVisible(false);
    root_layout->addWidget(diagnostics_group_);

    privacy_label_ = new QLabel{central_widget};
    privacy_label_->setObjectName(QStringLiteral("privacyLabel"));
    privacy_label_->setWordWrap(true);
    root_layout->addWidget(privacy_label_);
}

void MainWindow::OpenDocument(void) {
    QStringList patterns;
    for (const auto &format : registry_.importer_formats()) {
        for (const auto &extension : format.extensions) {
            patterns.emplace_back(QStringLiteral("*.%1").arg(Utf8(extension)));
        }
    }
    patterns.removeDuplicates();
    QFileDialog dialog{this, tr("Open Timeline")};
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters({tr("Supported timeline files (%1)")
                               .arg(patterns.isEmpty() ? QStringLiteral("*")
                                                       : patterns.join(u' ')),
                           tr("All files (*)")});
    dialog.resize(1000, 650);
    if (dialog.exec() == QDialog::Accepted) {
        const auto selected_files = dialog.selectedFiles();
        if (!selected_files.empty()) {
            OpenDocument(selected_files.front());
        }
    }
}

void MainWindow::OpenDocument(const QString &path) {
    if (path.isEmpty() || import_watcher_.isRunning()) {
        return;
    }
    StartImport(path);
}

void MainWindow::StartImport(const QString &path,
                             std::optional<std::string> frame_rate) {
    ClearDocument();
    current_path_ = path;
    requested_frame_rate_ = frame_rate;
    open_action_->setEnabled(false);
    empty_open_button_->setEnabled(false);
    document_stack_->setCurrentIndex(kLoadingPage);
    loading_label_->setText(tr("Opening %1…").arg(QFileInfo{path}.fileName()));

    services::OpenDocumentRequest request{
        .path = FilesystemPath(path),
    };
    if (frame_rate.has_value()) {
        request.options.emplace_back(core::MetadataEntry{
            .key = std::string{formats::cmx3600::kFrameRateOption},
            .value = *frame_rate,
        });
    }
    const auto *document_service = &document_service_;
    import_watcher_.setFuture(QtConcurrent::run(
        [document_service, request = std::move(request)](void) mutable {
            return document_service->OpenDocument(std::move(request));
        }));
}

void MainWindow::HandleImportFinished(void) {
    open_action_->setEnabled(true);
    empty_open_button_->setEnabled(true);
    auto result = import_watcher_.result();

    if (!result.has_value() &&
        result.error().kind ==
            services::DocumentOpenFailureKind::kImportFailed &&
        !requested_frame_rate_.has_value() &&
        HasDiagnosticCode(
            result.error().diagnostics,
            formats::cmx3600::diagnostic_code::kMissingFrameRate)) {
        const QStringList frame_rate_labels{
            tr("23.976 fps"), tr("24 fps"), tr("25 fps"),    tr("29.97 fps"),
            tr("30 fps"),     tr("50 fps"), tr("59.94 fps"), tr("60 fps"),
        };
        constexpr std::array<std::string_view, 8> kFrameRates{
            "24000/1001", "24", "25",         "30000/1001",
            "30",         "50", "60000/1001", "60",
        };
        bool accepted = false;
        const auto selected = QInputDialog::getItem(
            this, tr("Select Frame Rate"),
            tr("This non-drop-frame EDL does not declare its frame rate."),
            frame_rate_labels, 1, false, &accepted);
        if (accepted) {
            const auto index = frame_rate_labels.indexOf(selected);
            if (index >= 0) {
                StartImport(
                    PathText(result.error().path),
                    std::string{kFrameRates[static_cast<std::size_t>(index)]});
                return;
            }
        }
    }

    if (!result.has_value()) {
        ShowFailure(result.error());
        return;
    }

    document_ = std::move(result->document);
    last_diagnostics_ = std::move(result->diagnostics);
    last_load_error_.reset();
    last_filesystem_error_.clear();
    PopulateTimeline();
    PopulateDiagnostics(last_diagnostics_);
    document_stack_->setCurrentIndex(kTimelinePage);
    RememberRecentFile(PathText(result->path));
}

void MainWindow::ClearDocument(void) {
    event_model_->SetDocument(nullptr);
    document_.reset();
    current_path_.clear();
    requested_frame_rate_.reset();
    last_load_error_.reset();
    last_filesystem_error_.clear();
    last_diagnostics_.clear();
    event_filter_->clear();
    diagnostics_tree_->clear();
    diagnostics_group_->setVisible(false);
}

QString
MainWindow::DiagnosticMessage(const core::Diagnostic &diagnostic) const {
    const auto code = std::string_view{diagnostic.code};
    namespace cmx3600 = formats::cmx3600;
    if (code == cmx3600::diagnostic_code::kEncodingFallback) {
        return tr("The input was decoded as Windows-1252.");
    }
    if (code == cmx3600::diagnostic_code::kInvalidEncoding) {
        return tr("The input text encoding is invalid.");
    }
    if (code == cmx3600::diagnostic_code::kMissingFrameRate) {
        return tr("A frame rate is required for this non-drop-frame EDL.");
    }
    if (code == cmx3600::diagnostic_code::kInvalidFrameRate) {
        return tr("The selected frame rate is invalid or incompatible.");
    }
    if (code == cmx3600::diagnostic_code::kMalformedEvent) {
        return tr("An event record is malformed.");
    }
    if (code == cmx3600::diagnostic_code::kInvalidTimecode) {
        return tr("An event contains an invalid timecode or range.");
    }
    if (code == cmx3600::diagnostic_code::kUnknownContent) {
        return tr("Unrecognized CMX 3600 content was preserved.");
    }
    if (code == cmx3600::diagnostic_code::kOrphanRecord) {
        return tr("A motion-effect record has no preceding event.");
    }

    if (code == core::pipeline_diagnostic_code::kUnknownImportFormat) {
        return tr("No registered importer recognizes this file.");
    }
    if (code == core::pipeline_diagnostic_code::kAmbiguousImportFormat) {
        return tr("More than one importer matches this file.");
    }
    if (code == core::pipeline_diagnostic_code::kProbeException ||
        code == core::pipeline_diagnostic_code::kImportException) {
        return tr("The importer failed unexpectedly.");
    }
    if (code == core::pipeline_diagnostic_code::kImportProducedNoDocument) {
        return tr("The importer did not produce a timeline.");
    }
    return Utf8(diagnostic.message);
}

void MainWindow::PopulateTimeline(void) {
    if (!document_.has_value()) {
        return;
    }

    const auto &document = *document_;
    timeline_title_label_->setText(document.title.empty()
                                       ? QFileInfo{current_path_}.fileName()
                                       : Utf8(document.title));
    const auto rate = document.frame_rate.denominator() == 1
                          ? QString::number(document.frame_rate.numerator())
                          : QStringLiteral("%1/%2")
                                .arg(document.frame_rate.numerator())
                                .arg(document.frame_rate.denominator());
    timeline_summary_label_->setText(
        tr("%1 events · %2 fps · %3")
            .arg(static_cast<qulonglong>(document.events.size()))
            .arg(rate)
            .arg(document.timecode_mode == core::TimecodeMode::kDropFrame
                     ? tr("drop-frame")
                     : tr("non-drop-frame")));

    event_model_->SetDocument(&document);
    event_table_->resizeColumnsToContents();
}

void MainWindow::PopulateDiagnostics(
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
            {severity, line, DiagnosticMessage(diagnostic)},
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

void MainWindow::ShowFailure(const services::DocumentOpenFailure &failure) {
    event_model_->SetDocument(nullptr);
    document_.reset();
    current_path_ = PathText(failure.path);
    last_load_error_ = failure.kind;
    last_filesystem_error_ = failure.filesystem_error;
    last_diagnostics_ = failure.diagnostics;
    failure_title_label_->setText(tr("Could not open timeline"));

    if (failure.kind == services::DocumentOpenFailureKind::kOpenFailed) {
        failure_description_label_->setText(
            tr("The file could not be opened: %1")
                .arg(Utf8(last_filesystem_error_.message())));
    } else if (failure.kind == services::DocumentOpenFailureKind::kReadFailed) {
        failure_description_label_->setText(
            tr("The file could not be read: %1")
                .arg(Utf8(last_filesystem_error_.message())));
    } else {
        failure_description_label_->setText(
            tr("The file is not a supported timeline or contains fatal "
               "errors. Review the diagnostics below."));
    }
    PopulateDiagnostics(last_diagnostics_);
    document_stack_->setCurrentIndex(kFailurePage);
}

void MainWindow::RememberRecentFile(const QString &path) {
    if (!remember_recent_action_->isChecked()) {
        return;
    }

    QSettings settings;
    auto recent_files =
        settings.value(QStringLiteral("files/recent")).toStringList();
    recent_files.removeAll(path);
    recent_files.prepend(path);
    while (recent_files.size() > kMaximumRecentFiles) {
        recent_files.removeLast();
    }
    settings.setValue(QStringLiteral("files/recent"), recent_files);
    UpdateRecentFilesMenu();
}

void MainWindow::SetRememberRecentFiles(bool enabled) {
    QSettings settings;
    settings.setValue(QStringLiteral("files/rememberRecent"), enabled);
    if (!enabled) {
        settings.remove(QStringLiteral("files/recent"));
    }
    UpdateRecentFilesMenu();
}

void MainWindow::UpdateRecentFilesMenu(void) {
    recent_files_menu_->clear();
    if (!remember_recent_action_->isChecked()) {
        auto *disabled = recent_files_menu_->addAction(tr("History disabled"));
        disabled->setEnabled(false);
        return;
    }

    const QSettings settings;
    const auto recent_files =
        settings.value(QStringLiteral("files/recent")).toStringList();
    if (recent_files.empty()) {
        auto *empty = recent_files_menu_->addAction(tr("No recent files"));
        empty->setEnabled(false);
        return;
    }

    for (const auto &path : recent_files) {
        auto *action =
            recent_files_menu_->addAction(QFileInfo{path}.fileName());
        action->setToolTip(path);
        connect(action, &QAction::triggered, this,
                [this, path](void) { OpenDocument(path); });
    }
}

void MainWindow::ShowAboutDialog(void) {
    const auto version = QString::fromStdString(std::string{core::Version()});
    QMessageBox::about(
        this, tr("About Edit Atlas"),
        tr("Edit Atlas %1\n\nInspect editorial timelines and export "
           "structured reports.")
            .arg(version));
}

void MainWindow::ChangeLanguage(ApplicationLanguage language) {
    if (language == language_) {
        return;
    }

    const auto previous_language = language_;
    language_ = language;
    if (!SetApplicationLanguage(translator_, language_)) {
        language_ = previous_language;
        static_cast<void>(
            SetApplicationLanguage(translator_, previous_language));
        RetranslateUi();
        return;
    }

    SaveApplicationLanguage(language_);
    RetranslateUi();
}

void MainWindow::RetranslateUi(void) {
    setWindowTitle(tr("Edit Atlas"));
    file_menu_->setTitle(tr("&File"));
    open_action_->setText(tr("&Open Timeline…"));
    recent_files_menu_->setTitle(tr("Open &Recent"));
    remember_recent_action_->setText(tr("Remember Recent Files"));
    remember_recent_action_->setToolTip(
        tr("When enabled, Edit Atlas stores only the paths of opened files."));
    exit_action_->setText(tr("E&xit"));
    help_menu_->setTitle(tr("&Help"));
    about_action_->setText(tr("&About Edit Atlas"));

    const QSignalBlocker blocker{language_selector_};
    const auto language_value = static_cast<int>(language_);
    language_selector_->setCurrentIndex(
        language_selector_->findData(language_value));
    language_selector_->setToolTip(tr("Language"));
    language_selector_->setAccessibleName(tr("Language"));

    title_label_->setText(tr("Edit Atlas"));
    subtitle_label_->setText(tr("Private tools for editorial timelines."));
    empty_title_label_->setText(tr("No timeline open"));
    empty_description_label_->setText(
        tr("Open or drop a CMX 3600 EDL to inspect its edit events."));
    empty_open_button_->setText(tr("Open Timeline…"));
    loading_label_->setAccessibleName(tr("Opening timeline"));
    event_filter_->setPlaceholderText(tr("Filter events…"));
    event_filter_->setAccessibleName(tr("Filter timeline events"));
    event_table_->setAccessibleName(tr("Timeline edit events"));
    diagnostics_tree_->setAccessibleName(tr("Import diagnostics"));
    privacy_label_->setText(
        tr("Your media and timeline data stay on this computer."));

    failure_open_button_->setText(tr("Open Another Timeline…"));

    UpdateRecentFilesMenu();
    RetranslateTimeline();
}

void MainWindow::RetranslateTimeline(void) {
    if (document_.has_value()) {
        PopulateTimeline();
        PopulateDiagnostics(last_diagnostics_);
        return;
    }
    if (document_stack_->currentIndex() == kFailurePage) {
        services::DocumentOpenFailure failure{
            .path = FilesystemPath(current_path_),
            .kind = last_load_error_.value_or(
                services::DocumentOpenFailureKind::kImportFailed),
            .filesystem_error = last_filesystem_error_,
            .diagnostics = last_diagnostics_,
        };
        ShowFailure(failure);
    }
}

} // namespace edit_atlas::app
