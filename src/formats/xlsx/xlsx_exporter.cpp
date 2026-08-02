#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/formats/xlsx/detail/xlsx_workbook_text.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <xlsxwriter.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <span>
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

using detail::WorkbookText;

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

[[nodiscard]] std::string_view
TrackKindText(core::TrackKind kind, const WorkbookText &text) noexcept {
    switch (kind) {
    case core::TrackKind::kVideo:
        return text.video;
    case core::TrackKind::kAudio:
        return text.audio;
    case core::TrackKind::kData:
        return text.data;
    case core::TrackKind::kOther:
        return text.other;
    }
    return text.other;
}

[[nodiscard]] std::string_view EditTypeText(core::EditType type,
                                            const WorkbookText &text) noexcept {
    switch (type) {
    case core::EditType::kCut:
        return text.cut;
    case core::EditType::kDissolve:
        return text.dissolve;
    case core::EditType::kWipe:
        return text.wipe;
    case core::EditType::kKey:
        return text.key;
    case core::EditType::kOther:
        return text.other;
    }
    return text.other;
}

[[nodiscard]] std::string_view
DiagnosticSeverityText(core::DiagnosticSeverity severity,
                       const WorkbookText &text) noexcept {
    switch (severity) {
    case core::DiagnosticSeverity::kInfo:
        return text.info;
    case core::DiagnosticSeverity::kWarning:
        return text.warning;
    case core::DiagnosticSeverity::kError:
        return text.error;
    }
    return text.error;
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

[[nodiscard]] WorkbookLanguage
LanguageOption(const std::vector<core::MetadataEntry> &options) {
    const auto option = std::ranges::find(options, kWorkbookLanguageOption,
                                          &core::MetadataEntry::key);
    if (option == options.end()) {
        return WorkbookLanguage::kEnglish;
    }
    const auto *value = std::get_if<std::string>(&option->value);
    if (value != nullptr &&
        *value == WorkbookLanguageTag(WorkbookLanguage::kBrazilianPortuguese)) {
        return WorkbookLanguage::kBrazilianPortuguese;
    }
    return WorkbookLanguage::kEnglish;
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

void WriteBoolean(WriteStatus &status, lxw_worksheet *worksheet, lxw_row_t row,
                  lxw_col_t column, bool value, lxw_format *format = nullptr) {
    status.Record(
        worksheet_write_boolean(worksheet, row, column, value ? 1 : 0, format));
}

void WriteMetadataValue(WriteStatus &status, lxw_worksheet *worksheet,
                        lxw_row_t row, lxw_col_t column,
                        const core::MetadataValue &value) {
    std::visit(
        [&](const auto &item) {
            using Value = std::remove_cvref_t<decltype(item)>;
            if constexpr (std::is_same_v<Value, bool>) {
                WriteBoolean(status, worksheet, row, column, item);
            } else if constexpr (std::is_same_v<Value, std::string>) {
                WriteString(status, worksheet, row, column, item);
            } else {
                WriteNumber(status, worksheet, row, column,
                            static_cast<double>(item));
            }
        },
        value);
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
                          std::span<const core::TimelineEventField> projection,
                          lxw_format *wrapped_format, std::size_t event_count) {
    worksheet_freeze_panes(worksheet, 1, 0);
    for (std::size_t index = 0; index < projection.size(); ++index) {
        const auto field = projection[index];
        const auto column = static_cast<lxw_col_t>(index);
        const auto wide = field == core::TimelineEventField::kClipName ||
                          field == core::TimelineEventField::kSourceFile ||
                          field == core::TimelineEventField::kComments;
        const auto width = [&] {
            switch (field) {
            case core::TimelineEventField::kEventIdentifier:
            case core::TimelineEventField::kReel:
            case core::TimelineEventField::kSourceLine:
                return 12.0;
            case core::TimelineEventField::kTransitionIdentifier:
            case core::TimelineEventField::kTransitionDuration:
                return 18.0;
            case core::TimelineEventField::kDuration:
                return 16.0;
            case core::TimelineEventField::kClipName:
            case core::TimelineEventField::kSourceFile:
            case core::TimelineEventField::kComments:
                return 32.0;
            case core::TimelineEventField::kTrackKind:
            case core::TimelineEventField::kTrackIdentifier:
            case core::TimelineEventField::kEditType:
            case core::TimelineEventField::kSourceIn:
            case core::TimelineEventField::kSourceOut:
            case core::TimelineEventField::kRecordIn:
            case core::TimelineEventField::kRecordOut:
                return 14.0;
            }
            return 14.0;
        }();
        status.Record(worksheet_set_column(worksheet, column, column, width,
                                           wide ? wrapped_format : nullptr));
    }
    if (event_count > 0) {
        status.Record(worksheet_autofilter(
            worksheet, 0, 0, static_cast<lxw_row_t>(event_count),
            static_cast<lxw_col_t>(projection.size() - 1)));
    }
}

[[nodiscard]] std::string_view
EventColumnText(core::TimelineEventField field,
                const WorkbookText &text) noexcept {
    const auto projection = core::DefaultTimelineEventProjection();
    const auto item = std::ranges::find(projection, field);
    if (item == projection.end()) {
        return {};
    }
    return text
        .event_columns[static_cast<std::size_t>(item - projection.begin())];
}

void WriteEventField(WriteStatus &status, lxw_worksheet *worksheet,
                     lxw_row_t row, lxw_col_t column,
                     core::TimelineEventField field,
                     const core::EditEvent &event, const WorkbookText &text,
                     lxw_format *wrapped_format) {
    switch (field) {
    case core::TimelineEventField::kEventIdentifier:
        WriteString(status, worksheet, row, column, event.identifier);
        return;
    case core::TimelineEventField::kReel:
        WriteString(status, worksheet, row, column, event.reel);
        return;
    case core::TimelineEventField::kTrackKind:
        WriteString(status, worksheet, row, column,
                    TrackKindText(event.track.kind, text));
        return;
    case core::TimelineEventField::kTrackIdentifier:
        WriteString(status, worksheet, row, column, event.track.identifier);
        return;
    case core::TimelineEventField::kEditType:
        WriteString(status, worksheet, row, column,
                    EditTypeText(event.edit_type, text));
        return;
    case core::TimelineEventField::kTransitionIdentifier:
        if (event.transition.has_value()) {
            WriteString(status, worksheet, row, column,
                        event.transition->identifier);
        }
        return;
    case core::TimelineEventField::kTransitionDuration:
        if (event.transition.has_value()) {
            WriteNumber(status, worksheet, row, column,
                        static_cast<double>(event.transition->duration_frames));
        }
        return;
    case core::TimelineEventField::kSourceIn:
        WriteString(status, worksheet, row, column,
                    TimecodeText(event.source_range.start()));
        return;
    case core::TimelineEventField::kSourceOut:
        WriteString(status, worksheet, row, column,
                    TimecodeText(event.source_range.end_exclusive()));
        return;
    case core::TimelineEventField::kRecordIn:
        WriteString(status, worksheet, row, column,
                    TimecodeText(event.record_range.start()));
        return;
    case core::TimelineEventField::kRecordOut:
        WriteString(status, worksheet, row, column,
                    TimecodeText(event.record_range.end_exclusive()));
        return;
    case core::TimelineEventField::kDuration:
        WriteNumber(status, worksheet, row, column,
                    static_cast<double>(event.record_range.DurationInFrames()));
        return;
    case core::TimelineEventField::kClipName:
        WriteString(status, worksheet, row, column,
                    MetadataText(event.metadata, "clip_name"));
        return;
    case core::TimelineEventField::kSourceFile:
        WriteString(status, worksheet, row, column,
                    MetadataText(event.metadata, "source_file"));
        return;
    case core::TimelineEventField::kComments:
        WriteString(status, worksheet, row, column, CommentText(event.comments),
                    wrapped_format);
        return;
    case core::TimelineEventField::kSourceLine:
        if (event.provenance.has_value() &&
            event.provenance->location.line != 0) {
            WriteNumber(status, worksheet, row, column,
                        static_cast<double>(event.provenance->location.line));
        }
        return;
    }
}

void WriteEventsSheet(WriteStatus &status, lxw_worksheet *worksheet,
                      const core::TimelineDocument &document,
                      std::span<const core::TimelineEventField> projection,
                      const WorkbookText &text, lxw_format *header_format,
                      lxw_format *wrapped_format) {
    for (std::size_t column = 0; column < projection.size(); ++column) {
        WriteString(status, worksheet, 0, static_cast<lxw_col_t>(column),
                    EventColumnText(projection[column], text), header_format);
    }
    ConfigureEventsSheet(status, worksheet, projection, wrapped_format,
                         document.events.size());

    for (std::size_t index = 0; index < document.events.size(); ++index) {
        const auto &event = document.events[index];
        const auto row = static_cast<lxw_row_t>(index + 1);
        for (std::size_t column = 0; column < projection.size(); ++column) {
            WriteEventField(status, worksheet, row,
                            static_cast<lxw_col_t>(column), projection[column],
                            event, text, wrapped_format);
        }
    }
}

void WriteTimelineSheet(WriteStatus &status, lxw_worksheet *worksheet,
                        const core::TimelineDocument &document,
                        const WorkbookText &text, lxw_format *header_format) {
    WriteHeader(status, worksheet, text.timeline_columns, header_format);
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

    write_property(text.title, document.title);
    write_property(text.frame_rate,
                   IntegerText(document.frame_rate.numerator()) + "/" +
                       IntegerText(document.frame_rate.denominator()));
    write_property(text.timecode_mode,
                   document.timecode_mode == core::TimecodeMode::kDropFrame
                       ? text.drop_frame
                       : text.non_drop_frame);
    WriteString(status, worksheet, row, 0, text.event_count);
    WriteNumber(status, worksheet, row, 1,
                static_cast<double>(document.events.size()));
    ++row;

    for (const auto &entry : document.metadata) {
        WriteString(status, worksheet, row, 0, entry.key);
        WriteMetadataValue(status, worksheet, row, 1, entry.value);
        ++row;
    }

    status.Record(worksheet_autofilter(worksheet, 0, 0, row - 1, 1));
}

void WriteDiagnosticsSheet(WriteStatus &status, lxw_worksheet *worksheet,
                           const core::TimelineDocument &document,
                           const WorkbookText &text, lxw_format *header_format,
                           lxw_format *wrapped_format) {
    WriteHeader(status, worksheet, text.diagnostic_columns, header_format);
    worksheet_freeze_panes(worksheet, 1, 0);
    status.Record(worksheet_set_column(worksheet, 0, 1, 20.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 2, 2, 60.0, wrapped_format));
    status.Record(worksheet_set_column(worksheet, 3, 3, 32.0, nullptr));
    status.Record(worksheet_set_column(worksheet, 4, 5, 12.0, nullptr));

    for (std::size_t index = 0; index < document.diagnostics.size(); ++index) {
        const auto &diagnostic = document.diagnostics[index];
        const auto row = static_cast<lxw_row_t>(index + 1);
        WriteString(status, worksheet, row, 0,
                    DiagnosticSeverityText(diagnostic.severity, text));
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
    if (!core::IsValidTimelineEventProjection(request.event_projection)) {
        return core::ExportResult{
            .artifact = std::nullopt,
            .diagnostics = {ErrorDiagnostic(
                diagnostic_code::kInvalidEventProjection,
                "The event projection must contain at least one unique "
                "field.")},
        };
    }
    const auto &text = detail::WorkbookTextFor(LanguageOption(request.options));
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
        .subject = text.subject.data(),
        .author = "Edit Atlas",
        .manager = nullptr,
        .company = nullptr,
        .category = text.category.data(),
        .keywords = text.keywords.data(),
        .comments = text.comments.data(),
        .status = nullptr,
        .hyperlink_base = nullptr,
        .created = kCreationTime,
    };
    WriteStatus status{};
    status.Record(workbook_set_properties(workbook, &properties));

    auto *events = workbook_add_worksheet(workbook, text.events_sheet.data());
    const auto include_timeline =
        BooleanOption(request.options, kIncludeTimelineSheetOption, true);
    const auto include_diagnostics =
        BooleanOption(request.options, kIncludeDiagnosticsSheetOption, true);
    auto *timeline =
        include_timeline
            ? workbook_add_worksheet(workbook, text.timeline_sheet.data())
            : nullptr;
    auto *diagnostics =
        include_diagnostics
            ? workbook_add_worksheet(workbook, text.diagnostics_sheet.data())
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

    WriteEventsSheet(status, events, request.document, request.event_projection,
                     text, header_format, wrapped_format);
    if (timeline != nullptr) {
        WriteTimelineSheet(status, timeline, request.document, text,
                           header_format);
    }
    if (diagnostics != nullptr) {
        WriteDiagnosticsSheet(status, diagnostics, request.document, text,
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
