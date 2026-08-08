#ifndef EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_
#define EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/core/timeline_projection.hpp>

#include <span>
#include <string_view>

namespace edit_atlas::formats::xlsx::detail {

/// Identifies a localized XLSX label that is not an event projection field.
enum class WorkbookTextKey {
    kEventsSheet,
    kTimelineSheet,
    kDiagnosticsSheet,
    kSubject,
    kCategory,
    kKeywords,
    kComments,
    kTitle,
    kFrameRate,
    kTimecodeMode,
    kDropFrame,
    kNonDropFrame,
    kEventCount,
    kVideo,
    kAudio,
    kData,
    kOther,
    kCut,
    kDissolve,
    kWipe,
    kKey,
    kInfo,
    kWarning,
    kError,
    kTimelinePropertyColumn,
    kTimelineValueColumn,
    kDiagnosticSeverityColumn,
    kDiagnosticCodeColumn,
    kDiagnosticMessageColumn,
    kDiagnosticSourceColumn,
    kDiagnosticLineColumn,
    kDiagnosticColumnColumn,
    kCount,
};

class WorkbookText final {
  public:
    explicit WorkbookText(WorkbookLanguage language) noexcept;

    /// Returns the localized text for \p key.
    [[nodiscard]] std::string_view Get(WorkbookTextKey key) const noexcept;

    /// Returns the localized header for an event projection field.
    [[nodiscard]] std::string_view
    EventColumn(core::TimelineEventField field) const noexcept;

    /// Returns the ordered timeline-sheet header keys.
    [[nodiscard]] std::span<const WorkbookTextKey>
    TimelineColumns(void) const noexcept;

    /// Returns the ordered diagnostics-sheet header keys.
    [[nodiscard]] std::span<const WorkbookTextKey>
    DiagnosticColumns(void) const noexcept;

  private:
    WorkbookLanguage language_;
};

[[nodiscard]] const WorkbookText &
WorkbookTextFor(WorkbookLanguage language) noexcept;

} // namespace edit_atlas::formats::xlsx::detail

#endif // EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_
