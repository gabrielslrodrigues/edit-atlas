#ifndef EDIT_ATLAS_PRESENTATION_TIMELINE_DOCUMENT_VIEW_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_TIMELINE_DOCUMENT_VIEW_MODEL_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/presentation/diagnostic_model.hpp>
#include <edit_atlas/presentation/timeline_document_workflow.hpp>
#include <edit_atlas/presentation/timeline_event_model.hpp>

#include <edit_atlas/services/timeline_document_import_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>

#include <QObject>
#include <QtGlobal>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace edit_atlas::presentation {

/// Identifies the current document-loading state exposed to a frontend.
enum class TimelineDocumentState {
    /// No timeline has been imported.
    kEmpty,
    /// A timeline import is running.
    kImporting,
    /// A timeline is available for inspection and export.
    kReady,
    /// The most recent import failed.
    kImportFailed,
};

/// Identifies the current timeline-export state exposed to a frontend.
enum class TimelineExportState {
    /// No export is running.
    kIdle,
    /// A document export, optionally with rendered-video frames, is running.
    kExporting,
};

/// Explains why a requested presentation command was not started.
enum class TimelineDocumentCommandError {
    /// Another import or export operation is already running.
    kBusy,
    /// The command requires an imported timeline.
    kNoDocument,
    /// The active filter is invalid.
    kInvalidFilter,
    /// The selected event projection is empty, duplicated, or unknown.
    kInvalidProjection,
};

/// Result of asking the document ViewModel to start or apply an operation.
using TimelineDocumentCommandResult =
    std::expected<void, TimelineDocumentCommandError>;

/// Frontend-provided destination and format options for one export.
struct TimelineExportRequest final {
    /// Destination path selected by the frontend.
    std::filesystem::path path;
    /// Stable exporter identifier selected by the frontend.
    std::string format_identifier;
    /// Format-specific export options.
    std::vector<core::MetadataEntry> options;
    /// Optional rendered video used by image-bearing projections.
    std::optional<std::filesystem::path> video_path;
    /// Whether an existing destination may be replaced.
    bool replace_existing = false;
};

/// Owns one imported timeline and its frontend-neutral MVVM presentation state.
class TimelineDocumentViewModel final : public QObject {
    Q_OBJECT

  public:
    /// Creates empty presentation state using the supplied format registry.
    explicit TimelineDocumentViewModel(const core::FormatRegistry &registry,
                                       QObject *parent = nullptr);
    /// Destroys the ViewModel after its owned workflow has stopped.
    ~TimelineDocumentViewModel(void) override = default;

    /// ViewModels are non-copyable QObject owners.
    TimelineDocumentViewModel(const TimelineDocumentViewModel &) = delete;
    /// ViewModels are non-copy-assignable QObject owners.
    TimelineDocumentViewModel &
    operator=(const TimelineDocumentViewModel &) = delete;
    /// ViewModels are non-movable QObject owners.
    TimelineDocumentViewModel(TimelineDocumentViewModel &&) = delete;
    /// ViewModels are non-move-assignable QObject owners.
    TimelineDocumentViewModel &operator=(TimelineDocumentViewModel &&) = delete;

    /// Starts an asynchronous import after clearing the previous document.
    [[nodiscard]] TimelineDocumentCommandResult
    Import(services::TimelineDocumentImportRequest request);
    /// Clears all document-specific state when no operation is running.
    [[nodiscard]] TimelineDocumentCommandResult Clear(void);
    /// Applies a filter and updates the visible event selection.
    void SetFilterQuery(services::TimelineFilterQuery query);
    /// Selects the ordered event fields used by subsequent exports.
    [[nodiscard]] TimelineDocumentCommandResult
    SetEventProjection(std::vector<core::TimelineEventField> projection);
    /// Notifies item-model consumers that localized display text changed.
    void Retranslate(void);
    /// Starts an asynchronous export of the active filtered selection.
    [[nodiscard]] TimelineDocumentCommandResult
    Export(TimelineExportRequest request);
    /// Requests cooperative cancellation of a running rendered-video export.
    void CancelExport(void);

    /// Returns the current import and document-availability state.
    [[nodiscard]] TimelineDocumentState DocumentState(void) const noexcept;
    /// Returns whether an export is currently running.
    [[nodiscard]] TimelineExportState ExportState(void) const noexcept;
    /// Returns whether the ViewModel is importing or exporting.
    [[nodiscard]] bool IsBusy(void) const noexcept;
    /// Returns whether the current state can start an export.
    [[nodiscard]] bool CanExport(void) const noexcept;
    /// Returns the source path for the active or most recent import.
    [[nodiscard]] const std::filesystem::path &SourcePath(void) const noexcept;
    /// Returns the imported document, or null when none is available.
    [[nodiscard]] const core::TimelineDocument *Document(void) const noexcept;
    /// Returns non-fatal diagnostics from the successful import.
    [[nodiscard]] std::span<const core::Diagnostic>
    ImportDiagnostics(void) const noexcept;
    /// Returns the most recent import failure, or null after success or clear.
    [[nodiscard]] const services::TimelineDocumentImportFailure *
    ImportFailure(void) const noexcept;
    /// Returns the currently configured filter query.
    [[nodiscard]] const services::TimelineFilterQuery &
    FilterQuery(void) const noexcept;
    /// Returns source-document event indices selected by the active filter.
    [[nodiscard]] std::span<const std::size_t>
    EventSelection(void) const noexcept;
    /// Returns the active filter error, or null when the filter is valid.
    [[nodiscard]] const services::TimelineFilterError *
    FilterError(void) const noexcept;
    /// Returns the ordered event fields selected for export.
    [[nodiscard]] std::span<const core::TimelineEventField>
    EventProjection(void) const noexcept;
    /// Returns the mutable Qt item model presenting the selected events.
    [[nodiscard]] TimelineEventModel &EventModel(void) noexcept;
    /// Returns the Qt item model presenting the selected events.
    [[nodiscard]] const TimelineEventModel &EventModel(void) const noexcept;
    /// Returns the mutable Qt item model presenting import diagnostics.
    [[nodiscard]] DiagnosticModel &DiagnosticsModel(void) noexcept;
    /// Returns the Qt item model presenting import diagnostics.
    [[nodiscard]] const DiagnosticModel &DiagnosticsModel(void) const noexcept;
    /// Returns the most recent completed export result, or null before one.
    [[nodiscard]] const services::TimelineRenderedVideoExportResult *
    ExportResult(void) const noexcept;
    /// Returns the number of distinct event frames extracted so far.
    [[nodiscard]] qulonglong ExtractedFrameCount(void) const noexcept;
    /// Returns the total number of distinct event frames to extract.
    [[nodiscard]] qulonglong TotalFrameCount(void) const noexcept;

  signals:
    /// Reports a change to the active document, failure, or source context.
    void documentChanged(void);
    /// Reports a change to `DocumentState()`.
    void documentStateChanged(void);
    /// Reports that an export result is available.
    void exportFinished(void);
    /// Reports a change to `ExportState()`.
    void exportStateChanged(void);
    /// Reports a change to the query, selection, or filter error.
    void filterChanged(void);
    /// Reports a change to the ordered export fields.
    void eventProjectionChanged(void);
    /// Reports extracted and total distinct event-frame counts.
    void frameExtractionProgressChanged(qulonglong completed_events,
                                        qulonglong total_events);

  private:
    void ApplyFilter(void);
    void HandleImportFinished(void);
    void HandleExportFinished(void);
    void ResetDocument(void);
    void SetDocumentState(TimelineDocumentState state);
    void SetExportState(TimelineExportState state);

    TimelineDocumentWorkflow workflow_;
    TimelineEventModel event_model_;
    DiagnosticModel diagnostics_model_;
    TimelineDocumentState document_state_ = TimelineDocumentState::kEmpty;
    TimelineExportState export_state_ = TimelineExportState::kIdle;
    std::filesystem::path source_path_;
    std::optional<core::TimelineDocument> document_;
    std::vector<core::Diagnostic> import_diagnostics_;
    std::optional<services::TimelineDocumentImportFailure> import_failure_;
    services::TimelineFilterQuery filter_query_;
    services::TimelineEventSelection event_selection_;
    std::optional<services::TimelineFilterError> filter_error_;
    std::vector<core::TimelineEventField> event_projection_;
    std::optional<services::TimelineRenderedVideoExportResult> export_result_;
    qulonglong extracted_frame_count_ = 0;
    qulonglong total_frame_count_ = 0;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TIMELINE_DOCUMENT_VIEW_MODEL_HPP_
