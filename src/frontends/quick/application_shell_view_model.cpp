#include <edit_atlas/frontends/quick/application_shell_view_model.hpp>

#include <edit_atlas/presentation/application_state.hpp>
#include <edit_atlas/presentation/diagnostic_text.hpp>
#include <edit_atlas/presentation/timeline_document_view_model.hpp>
#include <edit_atlas/presentation/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/services/timeline_document_import_service.hpp>

#include <QAbstractItemModel>
#include <QFileInfo>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace edit_atlas::frontends::quick {
namespace {

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
HasDiagnosticCode(std::span<const core::Diagnostic> diagnostics,
                  std::string_view code) {
    return std::ranges::any_of(diagnostics,
                               [code](const core::Diagnostic &diagnostic) {
                                   return diagnostic.code == code;
                               });
}

} // namespace

ApplicationShellViewModel::ApplicationShellViewModel(
    const core::FormatRegistry &registry, QTranslator &translator,
    presentation::ApplicationLanguage initial_language, QObject *parent)
    : QObject{parent}, registry_{registry}, translator_{translator},
      language_{initial_language}, document_view_model_{registry_} {
    event_proxy_model_.setSourceModel(&document_view_model_.EventModel());
    event_proxy_model_.setSortRole(
        presentation::TimelineEventModel::kSortRole);
    event_proxy_model_.setDynamicSortFilter(true);
    connect(&document_view_model_,
            &presentation::TimelineDocumentViewModel::DocumentStateChanged,
            this, &ApplicationShellViewModel::HandleDocumentStateChanged);
    connect(&event_proxy_model_, &QAbstractItemModel::modelReset, this,
            [this] { emit DocumentPresentationChanged(); });
    connect(&document_view_model_.DiagnosticsModel(),
            &QAbstractItemModel::modelReset, this,
            [this] { emit DocumentPresentationChanged(); });
}

ApplicationShellViewModel::DocumentState
ApplicationShellViewModel::CurrentDocumentState(void) const noexcept {
    switch (document_view_model_.DocumentState()) {
    case presentation::TimelineDocumentState::kEmpty:
        return DocumentState::kEmpty;
    case presentation::TimelineDocumentState::kImporting:
        return DocumentState::kImporting;
    case presentation::TimelineDocumentState::kReady:
        return DocumentState::kReady;
    case presentation::TimelineDocumentState::kImportFailed:
        return DocumentState::kImportFailed;
    }
    return DocumentState::kEmpty;
}

bool ApplicationShellViewModel::IsEmpty(void) const noexcept {
    return CurrentDocumentState() == DocumentState::kEmpty;
}

bool ApplicationShellViewModel::IsBusy(void) const noexcept {
    return document_view_model_.IsBusy();
}

QString ApplicationShellViewModel::SourceFileName(void) const {
    return QFileInfo{PathText(document_view_model_.SourcePath())}.fileName();
}

qulonglong ApplicationShellViewModel::EventCount(void) const noexcept {
    const auto *document = document_view_model_.Document();
    return document == nullptr
               ? 0
               : static_cast<qulonglong>(document->events.size());
}

qulonglong ApplicationShellViewModel::VisibleEventCount(void) const noexcept {
    return static_cast<qulonglong>(event_proxy_model_.rowCount());
}

QString ApplicationShellViewModel::TimelineTitle(void) const {
    const auto *document = document_view_model_.Document();
    return document == nullptr || document->title.empty()
               ? SourceFileName()
               : QString::fromUtf8(
                     document->title.data(),
                     static_cast<qsizetype>(document->title.size()));
}

QString ApplicationShellViewModel::TimelineSummaryText(void) const {
    const auto *document = document_view_model_.Document();
    if (document == nullptr) {
        return {};
    }
    const auto &rate = document->frame_rate;
    const auto rate_text =
        rate.denominator() == 1
            ? QString::number(rate.numerator())
            : QStringLiteral("%1/%2")
                  .arg(rate.numerator())
                  .arg(rate.denominator());
    return tr("%1 events · %2 fps · %3")
        .arg(static_cast<qulonglong>(document->events.size()))
        .arg(rate_text)
        .arg(document->timecode_mode == core::TimecodeMode::kDropFrame
                 ? tr("drop-frame")
                 : tr("non-drop-frame"));
}

QAbstractItemModel *ApplicationShellViewModel::EventModel(void) noexcept {
    return &event_proxy_model_;
}

QAbstractItemModel *
ApplicationShellViewModel::DiagnosticsModel(void) noexcept {
    return &document_view_model_.DiagnosticsModel();
}

int ApplicationShellViewModel::DiagnosticCount(void) const noexcept {
    return document_view_model_.DiagnosticsModel().rowCount();
}

int ApplicationShellViewModel::EventSortColumn(void) const noexcept {
    return event_proxy_model_.sortColumn();
}

bool ApplicationShellViewModel::EventSortAscending(void) const noexcept {
    return event_proxy_model_.sortOrder() == Qt::AscendingOrder;
}

QString ApplicationShellViewModel::StatusText(void) const {
    switch (CurrentDocumentState()) {
    case DocumentState::kEmpty:
        return tr("Ready");
    case DocumentState::kImporting:
        return tr("Opening %1…").arg(SourceFileName());
    case DocumentState::kReady:
        return tr("Loaded %1").arg(SourceFileName());
    case DocumentState::kImportFailed:
        return tr("Could not open %1").arg(SourceFileName());
    }
    return {};
}

QString ApplicationShellViewModel::ErrorText(void) const {
    const auto *failure = document_view_model_.ImportFailure();
    return failure == nullptr
               ? QString{}
               : presentation::diagnostic_text::Summary(failure->diagnostics);
}

QStringList ApplicationShellViewModel::ImportFilePatterns(void) const {
    QStringList patterns;
    for (const auto &format : registry_.importer_formats()) {
        for (const auto &extension : format.extensions) {
            patterns.emplace_back(
                QStringLiteral("*.%1").arg(QString::fromStdString(extension)));
        }
    }
    patterns.removeDuplicates();
    return patterns;
}

QStringList ApplicationShellViewModel::RecentFiles(void) const {
    QStringList paths;
    const auto configured = presentation::ConfiguredRecentFiles();
    paths.reserve(static_cast<qsizetype>(configured.size()));
    for (const auto &path : configured) {
        paths.emplace_back(PathText(path));
    }
    return paths;
}

bool ApplicationShellViewModel::RememberRecentFiles(void) const {
    return presentation::RememberRecentFilesEnabled();
}

void ApplicationShellViewModel::SetRememberRecentFiles(bool enabled) {
    if (RememberRecentFiles() == enabled) {
        return;
    }
    presentation::SetRememberRecentFilesEnabled(enabled);
    emit RememberRecentFilesChanged();
    emit RecentFilesChanged();
}

QString ApplicationShellViewModel::LanguageCode(void) const {
    return language_ == presentation::ApplicationLanguage::kEnglish
               ? QStringLiteral("en")
               : QStringLiteral("pt_BR");
}

void ApplicationShellViewModel::SetLanguageCode(const QString &code) {
    const auto language =
        code == QStringLiteral("en")
            ? presentation::ApplicationLanguage::kEnglish
            : presentation::ApplicationLanguage::kBrazilianPortuguese;
    if (language_ == language) {
        return;
    }
    if (!presentation::SetApplicationLanguage(translator_, language)) {
        return;
    }
    language_ = language;
    presentation::SaveApplicationLanguage(language_);
    document_view_model_.Retranslate();
    emit LanguageChanged();
    emit StatusTextChanged();
    emit DocumentPresentationChanged();
}

void ApplicationShellViewModel::OpenUrl(const QUrl &url) {
    if (!url.isLocalFile()) {
        return;
    }
    OpenPath(url.toLocalFile());
}

void ApplicationShellViewModel::OpenPath(const QString &path) {
    if (path.isEmpty() || IsBusy()) {
        return;
    }
    StartImport(path, std::nullopt);
}

void ApplicationShellViewModel::RetryWithFrameRate(const QString &frame_rate) {
    const auto *failure = document_view_model_.ImportFailure();
    if (failure == nullptr || frame_rate.isEmpty() || IsBusy()) {
        return;
    }
    StartImport(PathText(failure->path), frame_rate.toStdString());
}

QString ApplicationShellViewModel::FileName(const QString &path) const {
    return QFileInfo{path}.fileName();
}

bool ApplicationShellViewModel::RequestClose(void) const noexcept {
    return !IsBusy();
}

void ApplicationShellViewModel::ToggleEventSort(int column) {
    if (column < 0 || column >= event_proxy_model_.columnCount()) {
        return;
    }
    const auto order =
        event_proxy_model_.sortColumn() == column &&
                event_proxy_model_.sortOrder() == Qt::AscendingOrder
            ? Qt::DescendingOrder
            : Qt::AscendingOrder;
    event_proxy_model_.sort(column, order);
    emit EventSortChanged();
}

void ApplicationShellViewModel::HandleDocumentStateChanged(void) {
    emit DocumentStateChanged();
    emit BusyChanged();
    emit DocumentPresentationChanged();
    emit StatusTextChanged();

    if (document_view_model_.DocumentState() ==
        presentation::TimelineDocumentState::kReady) {
        presentation::RecordRecentFile(document_view_model_.SourcePath());
        emit RecentFilesChanged();
        return;
    }

    const auto *failure = document_view_model_.ImportFailure();
    if (document_view_model_.DocumentState() !=
            presentation::TimelineDocumentState::kImportFailed ||
        failure == nullptr || requested_frame_rate_.has_value() ||
        failure->kind !=
            services::TimelineDocumentImportFailureKind::kImportFailed ||
        !HasDiagnosticCode(
            failure->diagnostics,
            formats::cmx3600::diagnostic_code::kMissingFrameRate)) {
        return;
    }
    emit frameRateRequired();
}

void ApplicationShellViewModel::StartImport(
    const QString &path, std::optional<std::string> frame_rate) {
    services::TimelineDocumentImportRequest request{
        .path = FilesystemPath(path),
        .format_identifier = {},
        .options = {},
    };
    if (frame_rate.has_value()) {
        request.options.emplace_back(core::MetadataEntry{
            .key = std::string{formats::cmx3600::kFrameRateOption},
            .value = *frame_rate,
        });
    }
    const auto result = document_view_model_.Import(std::move(request));
    if (!result.has_value()) {
        return;
    }
    requested_frame_rate_ = std::move(frame_rate);
}

} // namespace edit_atlas::frontends::quick
