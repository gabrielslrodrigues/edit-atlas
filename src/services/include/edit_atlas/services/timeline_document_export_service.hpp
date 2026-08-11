#ifndef EDIT_ATLAS_SERVICES_TIMELINE_DOCUMENT_EXPORT_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_DOCUMENT_EXPORT_SERVICE_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>
#include <edit_atlas/core/timeline_document_pipeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <expected>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace edit_atlas::services {

/// Describes one request to export a timeline to a local file.
struct TimelineDocumentExportRequest final {
    /// The requested destination path.
    std::filesystem::path path;
    /// The stable identifier of the exporter to invoke.
    std::string format_identifier;
    /// The complete timeline to export.
    core::TimelineDocument timeline;
    /// Ordered, unique event fields included in the exported representation.
    /// Defaults to every supported field in the standard report order.
    std::vector<core::TimelineEventField> event_projection{
        core::DefaultTimelineEventProjection().begin(),
        core::DefaultTimelineEventProjection().end(),
    };
    /// Format-specific export options.
    std::vector<core::MetadataEntry> options;
    /// Optional images associated with event positions in `timeline`.
    std::vector<core::TimelineEventImage> event_images{};
    /// Whether an existing destination may be atomically replaced.
    bool replace_existing;
};

/// Identifies the stage at which exporting a document failed.
enum class TimelineDocumentExportFailureKind {
    /// The request contains no fields or repeats an event field.
    kInvalidRequest,
    /// The selected exporter failed to produce an artifact.
    kExportFailed,
    /// The destination exists and replacement was not authorized.
    kDestinationExists,
    /// The artifact could not be written to a temporary sibling file.
    kWriteFailed,
    /// The completed temporary file could not replace the destination.
    kCommitFailed,
};

/// Contains the destination and diagnostics from a successful export.
struct TimelineDocumentExportReceipt final {
    /// The committed destination path.
    std::filesystem::path path;
    /// Non-fatal diagnostics produced by the exporter.
    std::vector<core::Diagnostic> diagnostics;
};

/// Contains a presentation-neutral exporter or filesystem failure.
struct TimelineDocumentExportFailure final {
    /// The requested destination path.
    std::filesystem::path path;
    /// The stage at which export failed.
    TimelineDocumentExportFailureKind kind;
    /// The native filesystem error, when failure involved the filesystem.
    std::error_code filesystem_error;
    /// Structured diagnostics produced by the pipeline or exporter.
    std::vector<core::Diagnostic> diagnostics;
};

/// Either a committed export receipt or a presentation-neutral failure.
using TimelineDocumentExportResult =
    std::expected<TimelineDocumentExportReceipt, TimelineDocumentExportFailure>;

/// Coordinates format-independent export with atomic local-file output.
///
/// The registry must outlive the service. Scheduling, overwrite confirmation,
/// and user feedback are responsibilities of the calling frontend.
class TimelineDocumentExportService final {
  public:
    /// Creates a service backed by a registry that must outlive it.
    explicit TimelineDocumentExportService(
        const core::FormatRegistry &registry) noexcept;

    /// Exports and atomically commits a timeline according to \p request.
    ///
    /// \returns A receipt on success or a stage-specific failure.
    [[nodiscard]] TimelineDocumentExportResult
    Export(TimelineDocumentExportRequest request) const;

  private:
    core::TimelineDocumentPipeline pipeline_;
};

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_TIMELINE_DOCUMENT_EXPORT_SERVICE_HPP_
