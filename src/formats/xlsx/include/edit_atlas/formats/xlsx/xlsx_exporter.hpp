#ifndef EDIT_ATLAS_FORMATS_XLSX_XLSX_EXPORTER_HPP_
#define EDIT_ATLAS_FORMATS_XLSX_XLSX_EXPORTER_HPP_

#include <edit_atlas/core/format.hpp>

#include <string_view>

namespace edit_atlas::formats::xlsx {

inline constexpr std::string_view kFormatIdentifier = "xlsx";
inline constexpr std::string_view kIncludeTimelineSheetOption =
    "xlsx.include_timeline_sheet";
inline constexpr std::string_view kIncludeDiagnosticsSheetOption =
    "xlsx.include_diagnostics_sheet";

namespace diagnostic_code {

inline constexpr std::string_view kWorkbookCreationFailed =
    "xlsx.workbook_creation_failed";
inline constexpr std::string_view kWorkbookWriteFailed =
    "xlsx.workbook_write_failed";

} // namespace diagnostic_code

/// Exports an editorial timeline as a structured Microsoft Excel workbook.
class XlsxExporter final : public core::Exporter {
  public:
    XlsxExporter(void) = default;
    ~XlsxExporter(void) override = default;

    [[nodiscard]] const core::FormatDescriptor &
    descriptor(void) const noexcept override;

    /// Returns workbook bytes without relying on caller-managed filesystem
    /// state.
    [[nodiscard]] core::ExportResult
    Export(const core::ExportRequest &request) const override;
};

} // namespace edit_atlas::formats::xlsx

#endif // EDIT_ATLAS_FORMATS_XLSX_XLSX_EXPORTER_HPP_
