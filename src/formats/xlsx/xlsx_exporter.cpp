#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <xlsxwriter.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace edit_atlas::formats::xlsx {
namespace {

constexpr std::string_view kMediaType =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
constexpr std::time_t kCreationTime = 946'684'800;

struct WriteStatus final {
    lxw_error error = LXW_NO_ERROR;

    void Record(lxw_error candidate) noexcept {
        if (error == LXW_NO_ERROR && candidate != LXW_NO_ERROR) {
            error = candidate;
        }
    }
};

[[nodiscard]] core::Diagnostic ErrorDiagnostic(std::string_view code,
                                               std::string message) {
    return core::Diagnostic{
        .severity = core::DiagnosticSeverity::kError,
        .code = std::string{code},
        .message = std::move(message),
        .location = std::nullopt,
    };
}

[[nodiscard]] std::string ErrorMessage(std::string_view prefix,
                                       lxw_error error) {
    std::string message{prefix};
    message += ": ";
    message += lxw_strerror(error);
    return message;
}

void AppendTwoDigits(std::string &output, std::int32_t value) {
    std::array<char, 16> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (result.ptr - buffer.data() < 2) {
        output.push_back('0');
    }
    output.append(buffer.data(), result.ptr);
}

[[nodiscard]] std::string TimecodeText(const core::Timecode &timecode) {
    std::string output;
    output.reserve(11);
    AppendTwoDigits(output, timecode.hours());
    output.push_back(':');
    AppendTwoDigits(output, timecode.minutes());
    output.push_back(':');
    AppendTwoDigits(output, timecode.seconds());
    output.push_back(timecode.mode() == core::TimecodeMode::kDropFrame ? ';'
                                                                       : ':');
    AppendTwoDigits(output, timecode.frames());
    return output;
}

template <typename Integer>
[[nodiscard]] std::string IntegerText(Integer value) {
    std::array<char, 32> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string{buffer.data(), result.ptr};
}

[[nodiscard]] std::string DoubleText(double value) {
    std::array<char, 64> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string{buffer.data(), result.ptr};
}

[[nodiscard]] std::string MetadataValueText(const core::MetadataValue &value) {
    return std::visit(
        [](const auto &item) -> std::string {
            using Value = std::remove_cvref_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, bool>) {
                return item ? "true" : "false";
            } else if constexpr (std::is_same_v<Value, std::string>) {
                return item;
            } else if constexpr (std::is_same_v<Value, double>) {
                return DoubleText(item);
            } else {
                return IntegerText(item);
            }
        },
        value);
}

[[nodiscard]] std::string_view TrackKindText(core::TrackKind kind) noexcept {
    switch (kind) {
    case core::TrackKind::kVideo:
        return "Video";
    case core::TrackKind::kAudio:
        return "Audio";
    case core::TrackKind::kData:
        return "Data";
    case core::TrackKind::kOther:
        return "Other";
    }
    return "Other";
}

[[nodiscard]] std::string_view EditTypeText(core::EditType type) noexcept {
    switch (type) {
    case core::EditType::kCut:
        return "Cut";
    case core::EditType::kDissolve:
        return "Dissolve";
    case core::EditType::kWipe:
        return "Wipe";
    case core::EditType::kKey:
        return "Key";
    case core::EditType::kOther:
        return "Other";
    }
    return "Other";
}

[[nodiscard]] std::string_view
DiagnosticSeverityText(core::DiagnosticSeverity severity) noexcept {
    switch (severity) {
    case core::DiagnosticSeverity::kInfo:
        return "Info";
    case core::DiagnosticSeverity::kWarning:
        return "Warning";
    case core::DiagnosticSeverity::kError:
        return "Error";
    }
    return "Error";
}

[[nodiscard]] std::string
MetadataText(const std::vector<core::MetadataEntry> &metadata,
             std::string_view key) {
    for (const auto &entry : metadata) {
        if (entry.key == key) {
            return MetadataValueText(entry.value);
        }
    }
    return {};
}

[[nodiscard]] std::string
CommentText(const std::vector<core::Comment> &comments) {
    std::string output;
    for (const auto &comment : comments) {
        if (!output.empty()) {
            output.push_back('\n');
        }
        output += comment.text;
    }
    return output;
}

[[nodiscard]] bool
BooleanOption(const std::vector<core::MetadataEntry> &options,
              std::string_view key, bool default_value) {
    const auto option =
        std::ranges::find(options, key, &core::MetadataEntry::key);
    if (option == options.end()) {
        return default_value;
    }
    if (const auto *value = std::get_if<bool>(&option->value);
        value != nullptr) {
        return *value;
    }
    return default_value;
}

void WriteString(WriteStatus &status, lxw_worksheet *worksheet, lxw_row_t row,
                 lxw_col_t column, std::string_view value,
                 lxw_format *format = nullptr) {
    const std::string text{value};
    status.Record(
        worksheet_write_string(worksheet, row, column, text.c_str(), format));
}

void WriteNumber(WriteStatus &status, lxw_worksheet *worksheet, lxw_row_t row,
                 lxw_col_t column, double value, lxw_format *format = nullptr) {
    status.Record(
        worksheet_write_number(worksheet, row, column, value, format));
}

template <std::size_t Size>
void WriteHeader(WriteStatus &status, lxw_worksheet *worksheet,
                 const std::array<std::string_view, Size> &columns,
                 lxw_format *format) {
    for (std::size_t column = 0; column < columns.size(); ++column) {
        WriteString(status, worksheet, 0, static_cast<lxw_col_t>(column),
                    columns[column], format);
    }
}

void ConfigureEventsSheet(WriteStatus &status, lxw_worksheet *worksheet,
                          lxw_format *wrapped_format, std::size_t event_count) {
    worksheet_freeze_panes(worksheet, 1, 0);
    status.Record(worksheet_set_column(worksheet, 0, 1, 12.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 2, 4, 14.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 5, 6, 18.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 7, 10, 14.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 11, 11, 16.0, nullptr));
    status.Record(
        worksheet_set_column(worksheet, 12, 14, 32.0, wrapped_format));
    status.Record(worksheet_set_column(worksheet, 15, 15, 12.0, nullptr));
    if (event_count > 0) {
        status.Record(worksheet_autofilter(
            worksheet, 0, 0, static_cast<lxw_row_t>(event_count), 15));
    }
}

void WriteEventsSheet(WriteStatus &status, lxw_worksheet *worksheet,
                      const core::TimelineDocument &document,
                      lxw_format *header_format, lxw_format *wrapped_format) {
    constexpr std::array<std::string_view, 16> kColumns{
        "Event",      "Reel",        "Track Type",        "Track",
        "Edit Type",  "Transition",  "Transition Frames", "Source In",
        "Source Out", "Record In",   "Record Out",        "Duration Frames",
        "Clip Name",  "Source File", "Comments",          "Source Line",
    };
    WriteHeader(status, worksheet, kColumns, header_format);
    ConfigureEventsSheet(status, worksheet, wrapped_format,
                         document.events.size());

    for (std::size_t index = 0; index < document.events.size(); ++index) {
        const auto &event = document.events[index];
        const auto row = static_cast<lxw_row_t>(index + 1);
        WriteString(status, worksheet, row, 0, event.identifier);
        WriteString(status, worksheet, row, 1, event.reel);
        WriteString(status, worksheet, row, 2, TrackKindText(event.track.kind));
        WriteString(status, worksheet, row, 3, event.track.identifier);
        WriteString(status, worksheet, row, 4, EditTypeText(event.edit_type));
        if (event.transition.has_value()) {
            WriteString(status, worksheet, row, 5,
                        event.transition->identifier);
            WriteNumber(status, worksheet, row, 6,
                        static_cast<double>(event.transition->duration_frames));
        }
        WriteString(status, worksheet, row, 7,
                    TimecodeText(event.source_range.start()));
        WriteString(status, worksheet, row, 8,
                    TimecodeText(event.source_range.end_exclusive()));
        WriteString(status, worksheet, row, 9,
                    TimecodeText(event.record_range.start()));
        WriteString(status, worksheet, row, 10,
                    TimecodeText(event.record_range.end_exclusive()));
        WriteNumber(status, worksheet, row, 11,
                    static_cast<double>(event.record_range.DurationInFrames()));
        WriteString(status, worksheet, row, 12,
                    MetadataText(event.metadata, "clip_name"));
        WriteString(status, worksheet, row, 13,
                    MetadataText(event.metadata, "source_file"));
        WriteString(status, worksheet, row, 14, CommentText(event.comments),
                    wrapped_format);
        if (event.provenance.has_value() &&
            event.provenance->location.line != 0) {
            WriteNumber(status, worksheet, row, 15,
                        static_cast<double>(event.provenance->location.line));
        }
    }
}

void WriteTimelineSheet(WriteStatus &status, lxw_worksheet *worksheet,
                        const core::TimelineDocument &document,
                        lxw_format *header_format) {
    constexpr std::array<std::string_view, 2> kColumns{"Property", "Value"};
    WriteHeader(status, worksheet, kColumns, header_format);
    worksheet_freeze_panes(worksheet, 1, 0);
    status.Record(worksheet_set_column(worksheet, 0, 0, 28.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 1, 1, 48.0, nullptr));

    lxw_row_t row = 1;
    const auto write_property = [&](std::string_view key,
                                    std::string_view value) {
        WriteString(status, worksheet, row, 0, key);
        WriteString(status, worksheet, row, 1, value);
        ++row;
    };

    write_property("Title", document.title);
    write_property("Frame Rate",
                   IntegerText(document.frame_rate.numerator()) + "/" +
                       IntegerText(document.frame_rate.denominator()));
    write_property("Timecode Mode",
                   document.timecode_mode == core::TimecodeMode::kDropFrame
                       ? "Drop Frame"
                       : "Non-Drop Frame");
    write_property("Event Count", IntegerText(document.events.size()));

    for (const auto &entry : document.metadata) {
        write_property(entry.key, MetadataValueText(entry.value));
    }

    status.Record(worksheet_autofilter(worksheet, 0, 0, row - 1, 1));
}

void WriteDiagnosticsSheet(WriteStatus &status, lxw_worksheet *worksheet,
                           const core::TimelineDocument &document,
                           lxw_format *header_format,
                           lxw_format *wrapped_format) {
    constexpr std::array<std::string_view, 6> kColumns{
        "Severity", "Code", "Message", "Source", "Line", "Column",
    };
    WriteHeader(status, worksheet, kColumns, header_format);
    worksheet_freeze_panes(worksheet, 1, 0);
    status.Record(worksheet_set_column(worksheet, 0, 1, 20.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 2, 2, 60.0, wrapped_format));
    status.Record(worksheet_set_column(worksheet, 3, 3, 32.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 4, 5, 12.0, nullptr));

    for (std::size_t index = 0; index < document.diagnostics.size(); ++index) {
        const auto &diagnostic = document.diagnostics[index];
        const auto row = static_cast<lxw_row_t>(index + 1);
        WriteString(status, worksheet, row, 0,
                    DiagnosticSeverityText(diagnostic.severity));
        WriteString(status, worksheet, row, 1, diagnostic.code);
        WriteString(status, worksheet, row, 2, diagnostic.message,
                    wrapped_format);
        if (diagnostic.location.has_value()) {
            WriteString(status, worksheet, row, 3, diagnostic.location->source);
            if (diagnostic.location->line != 0) {
                WriteNumber(status, worksheet, row, 4,
                            static_cast<double>(diagnostic.location->line));
            }
            if (diagnostic.location->column != 0) {
                WriteNumber(status, worksheet, row, 5,
                            static_cast<double>(diagnostic.location->column));
            }
        }
    }

    if (!document.diagnostics.empty()) {
        status.Record(worksheet_autofilter(
            worksheet, 0, 0,
            static_cast<lxw_row_t>(document.diagnostics.size()), 5));
    }
}

void FreeOutputBuffer(const char *buffer) noexcept {
    std::free(const_cast<char *>(buffer));
}

} // namespace

const core::FormatDescriptor &XlsxExporter::descriptor(void) const noexcept {
    static const core::FormatDescriptor kDescriptor{
        .identifier = std::string{kFormatIdentifier},
        .display_name = "Excel Workbook",
        .extensions = {"xlsx"},
    };
    return kDescriptor;
}

core::ExportResult
XlsxExporter::Export(const core::ExportRequest &request) const {
    const char *output_buffer = nullptr;
    std::size_t output_size = 0;
    lxw_workbook_options options{
        .constant_memory = LXW_FALSE,
        .tmpdir = nullptr,
        .use_zip64 = LXW_FALSE,
        .output_buffer = &output_buffer,
        .output_buffer_size = &output_size,
    };
    auto *workbook = workbook_new_opt(nullptr, &options);
    if (workbook == nullptr) {
        return core::ExportResult{
            .artifact = std::nullopt,
            .diagnostics = {ErrorDiagnostic(
                diagnostic_code::kWorkbookCreationFailed,
                "Could not create the Excel workbook.")},
        };
    }

    lxw_doc_properties properties{
        .title = request.document.title.c_str(),
        .subject = "Editorial timeline report",
        .author = "Edit Atlas",
        .manager = nullptr,
        .company = nullptr,
        .category = "Editorial",
        .keywords = "EDL, editorial, timeline",
        .comments = "Created by Edit Atlas",
        .status = nullptr,
        .hyperlink_base = nullptr,
        .created = kCreationTime,
    };
    WriteStatus status{};
    status.Record(workbook_set_properties(workbook, &properties));

    auto *events = workbook_add_worksheet(workbook, "Events");
    const auto include_timeline =
        BooleanOption(request.options, kIncludeTimelineSheetOption, true);
    const auto include_diagnostics =
        BooleanOption(request.options, kIncludeDiagnosticsSheetOption, true);
    auto *timeline = include_timeline
                         ? workbook_add_worksheet(workbook, "Timeline")
                         : nullptr;
    auto *diagnostics = include_diagnostics
                            ? workbook_add_worksheet(workbook, "Diagnostics")
                            : nullptr;
    auto *header_format = workbook_add_format(workbook);
    auto *wrapped_format = workbook_add_format(workbook);
    if (events == nullptr || (include_timeline && timeline == nullptr) ||
        (include_diagnostics && diagnostics == nullptr) ||
        header_format == nullptr || wrapped_format == nullptr) {
        static_cast<void>(workbook_close(workbook));
        FreeOutputBuffer(output_buffer);
        return core::ExportResult{
            .artifact = std::nullopt,
            .diagnostics = {ErrorDiagnostic(
                diagnostic_code::kWorkbookCreationFailed,
                "Could not create the Excel workbook structure.")},
        };
    }

    format_set_bold(header_format);
    format_set_font_color(header_format, LXW_COLOR_WHITE);
    format_set_bg_color(header_format, 0x1F4E78);
    format_set_border(header_format, LXW_BORDER_THIN);
    format_set_text_wrap(wrapped_format);
    format_set_align(wrapped_format, LXW_ALIGN_VERTICAL_TOP);

    WriteEventsSheet(status, events, request.document, header_format,
                     wrapped_format);
    if (timeline != nullptr) {
        WriteTimelineSheet(status, timeline, request.document, header_format);
    }
    if (diagnostics != nullptr) {
        WriteDiagnosticsSheet(status, diagnostics, request.document,
                              header_format, wrapped_format);
    }

    const auto close_error = workbook_close(workbook);
    status.Record(close_error);
    if (status.error != LXW_NO_ERROR) {
        FreeOutputBuffer(output_buffer);
        return core::ExportResult{
            .artifact = std::nullopt,
            .diagnostics = {ErrorDiagnostic(
                diagnostic_code::kWorkbookWriteFailed,
                ErrorMessage("Could not write the Excel workbook",
                             status.error))},
        };
    }
    if (output_buffer == nullptr || output_size == 0) {
        FreeOutputBuffer(output_buffer);
        return core::ExportResult{
            .artifact = std::nullopt,
            .diagnostics = {ErrorDiagnostic(
                diagnostic_code::kWorkbookWriteFailed,
                "The Excel workbook did not produce any output.")},
        };
    }

    const auto *begin = reinterpret_cast<const std::byte *>(output_buffer);
    std::vector<std::byte> content{begin, begin + output_size};
    FreeOutputBuffer(output_buffer);
    return core::ExportResult{
        .artifact =
            core::ExportArtifact{
                .content = std::move(content),
                .suggested_extension = "xlsx",
                .media_type = std::string{kMediaType},
            },
        .diagnostics = {},
    };
}

} // namespace edit_atlas::formats::xlsx
