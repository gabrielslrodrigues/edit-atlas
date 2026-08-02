#ifndef EDIT_ATLAS_FORMATS_XLSX_XLSX_EXPORTER_HPP_
#define EDIT_ATLAS_FORMATS_XLSX_XLSX_EXPORTER_HPP_

#include <edit_atlas/core/format.hpp>

#include <string_view>

namespace edit_atlas::formats::xlsx {

/// Stable identifier for the XLSX exporter.
inline constexpr std::string_view kFormatIdentifier = "xlsx";
/// Export option controlling inclusion of the timeline summary sheet.
inline constexpr std::string_view kIncludeTimelineSheetOption =
    "xlsx.include_timeline_sheet";
/// Export option controlling inclusion of the diagnostics sheet.
inline constexpr std::string_view kIncludeDiagnosticsSheetOption =
    "xlsx.include_diagnostics_sheet";
/// Export option containing an IETF workbook presentation language tag.
inline constexpr std::string_view kWorkbookLanguageOption =
    "xlsx.workbook_language";

/// A presentation language supported by the XLSX report.
enum class WorkbookLanguage {
    /// English workbook labels and document properties.
    kEnglish,
    /// Brazilian Portuguese workbook labels and document properties.
    kBrazilianPortuguese,
};

/// Returns the stable IETF language tag stored in export options.
[[nodiscard]] constexpr std::string_view
WorkbookLanguageTag(WorkbookLanguage language) noexcept {
    switch (language) {
    case WorkbookLanguage::kEnglish:
        return "en";
    case WorkbookLanguage::kBrazilianPortuguese:
        return "pt-BR";
    }
    return "en";
}

/// Stable diagnostic codes emitted by the XLSX exporter.
namespace diagnostic_code {

/// The in-memory workbook could not be created.
inline constexpr std::string_view kWorkbookCreationFailed =
    "xlsx.workbook_creation_failed";
/// The completed workbook could not be serialized.
inline constexpr std::string_view kWorkbookWriteFailed =
    "xlsx.workbook_write_failed";
/// The event projection is empty or contains duplicate fields.
inline constexpr std::string_view kInvalidEventProjection =
    "xlsx.invalid_event_projection";

} // namespace diagnostic_code

/// Exports an editorial timeline as a structured Microsoft Excel workbook.
class XlsxExporter final : public core::Exporter {
  public:
    /// Creates a stateless XLSX exporter.
    XlsxExporter(void) = default;
    /// Destroys the exporter.
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
