#ifndef EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_
#define EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <span>
#include <string_view>

namespace edit_atlas::formats::xlsx::detail {

struct WorkbookText final {
    std::string_view events_sheet;
    std::string_view timeline_sheet;
    std::string_view diagnostics_sheet;
    std::string_view subject;
    std::string_view category;
    std::string_view keywords;
    std::string_view comments;
    std::span<const std::string_view> event_columns;
    std::span<const std::string_view> timeline_columns;
    std::span<const std::string_view> diagnostic_columns;
    std::string_view title;
    std::string_view frame_rate;
    std::string_view timecode_mode;
    std::string_view drop_frame;
    std::string_view non_drop_frame;
    std::string_view event_count;
    std::string_view video;
    std::string_view audio;
    std::string_view data;
    std::string_view other;
    std::string_view cut;
    std::string_view dissolve;
    std::string_view wipe;
    std::string_view key;
    std::string_view info;
    std::string_view warning;
    std::string_view error;
};

[[nodiscard]] const WorkbookText &
WorkbookTextFor(WorkbookLanguage language) noexcept;

} // namespace edit_atlas::formats::xlsx::detail

#endif // EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_
