#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace edit_atlas::formats::cmx3600 {
namespace {

using core::Diagnostic;
using core::DiagnosticSeverity;
using core::FrameRate;
using core::ImportRequest;
using core::MetadataEntry;
using core::SourceLineProvenance;
using core::SourceLocation;
using core::Timecode;
using core::TimecodeMode;

struct DecodedText final {
    std::string text;
    bool used_encoding_fallback;
};

[[nodiscard]] unsigned char ByteValue(std::byte value) noexcept {
    return std::to_integer<unsigned char>(value);
}

void AppendUtf8(std::string &output, std::uint32_t code_point) {
    if (code_point <= 0x7F) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (code_point >> 6)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else if (code_point <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (code_point >> 12)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (code_point >> 18)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3F)));
    }
}

[[nodiscard]] bool IsValidUtf8(std::span<const std::byte> input) noexcept {
    std::size_t index = 0;
    while (index < input.size()) {
        const auto lead = ByteValue(input[index]);
        if (lead <= 0x7F) {
            ++index;
            continue;
        }

        std::size_t continuation_count = 0;
        std::uint32_t code_point = 0;
        if (lead >= 0xC2 && lead <= 0xDF) {
            continuation_count = 1;
            code_point = lead & 0x1F;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            continuation_count = 2;
            code_point = lead & 0x0F;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            continuation_count = 3;
            code_point = lead & 0x07;
        } else {
            return false;
        }

        if (index + continuation_count >= input.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
            const auto continuation = ByteValue(input[index + offset]);
            if ((continuation & 0xC0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | (continuation & 0x3F);
        }

        if ((continuation_count == 2 &&
             (code_point < 0x800 ||
              (code_point >= 0xD800 && code_point <= 0xDFFF))) ||
            (continuation_count == 3 &&
             (code_point < 0x10000 || code_point > 0x10FFFF))) {
            return false;
        }
        index += continuation_count + 1;
    }
    return true;
}

[[nodiscard]] std::optional<std::string>
DecodeUtf16(std::span<const std::byte> input, bool little_endian) {
    if (input.size() % 2 != 0) {
        return std::nullopt;
    }

    std::string output;
    output.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); index += 2) {
        const auto first = ByteValue(input[index]);
        const auto second = ByteValue(input[index + 1]);
        auto unit = static_cast<std::uint16_t>(
            little_endian ? first | (second << 8) : (first << 8) | second);
        std::uint32_t code_point = unit;

        if (unit >= 0xD800 && unit <= 0xDBFF) {
            if (index + 3 >= input.size()) {
                return std::nullopt;
            }
            const auto third = ByteValue(input[index + 2]);
            const auto fourth = ByteValue(input[index + 3]);
            const auto low = static_cast<std::uint16_t>(
                little_endian ? third | (fourth << 8) : (third << 8) | fourth);
            if (low < 0xDC00 || low > 0xDFFF) {
                return std::nullopt;
            }
            code_point =
                0x10000 + ((static_cast<std::uint32_t>(unit - 0xD800) << 10) |
                           (low - 0xDC00));
            index += 2;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            return std::nullopt;
        }
        AppendUtf8(output, code_point);
    }
    return output;
}

[[nodiscard]] std::string DecodeWindows1252(std::span<const std::byte> input) {
    constexpr std::array<std::uint16_t, 32> kControlCodePoints{{
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    }};

    std::string output;
    output.reserve(input.size());
    for (const auto byte : input) {
        const auto value = ByteValue(byte);
        const auto code_point =
            value >= 0x80 && value <= 0x9F
                ? kControlCodePoints[static_cast<std::size_t>(value - 0x80)]
                : value;
        AppendUtf8(output, code_point);
    }
    return output;
}

[[nodiscard]] std::optional<DecodedText>
DecodeText(std::span<const std::byte> input) {
    if (input.size() >= 3 && ByteValue(input[0]) == 0xEF &&
        ByteValue(input[1]) == 0xBB && ByteValue(input[2]) == 0xBF) {
        input = input.subspan(3);
    } else if (input.size() >= 2 && ByteValue(input[0]) == 0xFF &&
               ByteValue(input[1]) == 0xFE) {
        auto text = DecodeUtf16(input.subspan(2), true);
        return text.has_value() ? std::optional<DecodedText>{DecodedText{
                                      std::move(*text), false}}
                                : std::nullopt;
    } else if (input.size() >= 2 && ByteValue(input[0]) == 0xFE &&
               ByteValue(input[1]) == 0xFF) {
        auto text = DecodeUtf16(input.subspan(2), false);
        return text.has_value() ? std::optional<DecodedText>{DecodedText{
                                      std::move(*text), false}}
                                : std::nullopt;
    }

    if (IsValidUtf8(input)) {
        std::string text;
        text.reserve(input.size());
        for (const auto byte : input) {
            text.push_back(static_cast<char>(ByteValue(byte)));
        }
        return DecodedText{std::move(text), false};
    }
    return DecodedText{DecodeWindows1252(input), true};
}

[[nodiscard]] std::string_view Trim(std::string_view value) noexcept {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

[[nodiscard]] std::vector<std::string_view> Tokens(std::string_view line) {
    std::vector<std::string_view> tokens;
    while (!line.empty()) {
        line = Trim(line);
        if (line.empty()) {
            break;
        }
        const auto separator = line.find_first_of(" \t");
        tokens.emplace_back(line.substr(0, separator));
        line = separator == std::string_view::npos ? std::string_view{}
                                                   : line.substr(separator + 1);
    }
    return tokens;
}

template <typename Integer>
[[nodiscard]] bool ParseInteger(std::string_view text,
                                Integer &value) noexcept {
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

[[nodiscard]] std::optional<FrameRate>
ParseFrameRate(const ImportRequest &request) {
    for (const auto &option : request.options) {
        if (option.key != kFrameRateOption) {
            continue;
        }
        if (const auto *integer = std::get_if<std::int64_t>(&option.value)) {
            const auto rate = FrameRate::Create(*integer, 1);
            return rate.has_value() ? std::optional<FrameRate>{*rate}
                                    : std::nullopt;
        }
        const auto *text = std::get_if<std::string>(&option.value);
        if (text == nullptr) {
            return std::nullopt;
        }
        const auto separator = text->find('/');
        std::int64_t numerator = 0;
        std::int64_t denominator = 1;
        if (!ParseInteger<std::int64_t>(
                separator == std::string::npos
                    ? std::string_view{*text}
                    : std::string_view{*text}.substr(0, separator),
                numerator) ||
            (separator != std::string::npos &&
             !ParseInteger<std::int64_t>(
                 std::string_view{*text}.substr(separator + 1), denominator))) {
            return std::nullopt;
        }
        const auto rate = FrameRate::Create(numerator, denominator);
        return rate.has_value() ? std::optional<FrameRate>{*rate}
                                : std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] bool HasFrameRateOption(const ImportRequest &request) {
    return std::ranges::any_of(request.options,
                               [](const MetadataEntry &option) {
                                   return option.key == kFrameRateOption;
                               });
}

[[nodiscard]] SourceLocation Location(const ImportRequest &request,
                                      std::uint64_t line) {
    return SourceLocation{
        .source = request.source_name,
        .line = line,
        .column = 1,
    };
}

void AddDiagnostic(std::vector<Diagnostic> &diagnostics,
                   DiagnosticSeverity severity, std::string_view code,
                   std::string message, const ImportRequest &request,
                   std::uint64_t line = 0) {
    diagnostics.emplace_back(Diagnostic{
        .severity = severity,
        .code = std::string{code},
        .message = std::move(message),
        .location =
            line == 0 ? std::nullopt
                      : std::optional<SourceLocation>{Location(request, line)},
    });
}

[[nodiscard]] std::optional<Timecode>
ParseTimecode(std::string_view text, const FrameRate &rate, TimecodeMode mode) {
    if (text.size() != 11 || text[2] != ':' || text[5] != ':' ||
        (text[8] != ':' && text[8] != ';')) {
        return std::nullopt;
    }
    std::int64_t hours = 0;
    std::int64_t minutes = 0;
    std::int64_t seconds = 0;
    std::int64_t frames = 0;
    if (!ParseInteger(text.substr(0, 2), hours) ||
        !ParseInteger(text.substr(3, 2), minutes) ||
        !ParseInteger(text.substr(6, 2), seconds) ||
        !ParseInteger(text.substr(9, 2), frames)) {
        return std::nullopt;
    }
    const auto timecode =
        Timecode::Create(hours, minutes, seconds, frames, rate, mode);
    return timecode.has_value() ? std::optional<Timecode>{*timecode}
                                : std::nullopt;
}

[[nodiscard]] bool IsEventNumber(std::string_view token) noexcept {
    return !token.empty() && std::ranges::all_of(token, [](char character) {
        return character >= '0' && character <= '9';
    });
}

[[nodiscard]] core::TrackKind TrackKind(std::string_view token) noexcept {
    if (token == "V") {
        return core::TrackKind::kVideo;
    }
    if (token.starts_with('A')) {
        return core::TrackKind::kAudio;
    }
    return core::TrackKind::kOther;
}

[[nodiscard]] SourceLineProvenance Provenance(const ImportRequest &request,
                                              std::uint64_t line_number,
                                              std::string_view line) {
    return SourceLineProvenance{
        .location = Location(request, line_number),
        .line = std::string{line},
    };
}

void AddComment(core::EditEvent &event, const ImportRequest &request,
                std::uint64_t line_number, std::string_view line) {
    const auto text = Trim(line.substr(1));
    event.comments.emplace_back(core::Comment{
        .text = std::string{text},
        .provenance = Provenance(request, line_number, line),
    });

    constexpr std::string_view kClipName = "FROM CLIP NAME:";
    constexpr std::string_view kSourceFile = "SOURCE FILE:";
    if (text.starts_with(kClipName)) {
        event.metadata.emplace_back(
            "clip_name", std::string{Trim(text.substr(kClipName.size()))});
    } else if (text.starts_with(kSourceFile)) {
        event.metadata.emplace_back(
            "source_file", std::string{Trim(text.substr(kSourceFile.size()))});
    } else {
        event.metadata.emplace_back("cmx3600.comment", std::string{text});
    }
}

} // namespace

const core::FormatDescriptor &Cmx3600Importer::descriptor(void) const noexcept {
    static const core::FormatDescriptor kDescriptor{
        .identifier = std::string{kFormatIdentifier},
        .display_name = "CMX 3600 EDL",
        .extensions = {"edl"},
    };
    return kDescriptor;
}

core::ProbeConfidence
Cmx3600Importer::Probe(const core::ImportRequest &request) const {
    const auto decoded = DecodeText(request.content);
    if (!decoded.has_value()) {
        return core::ProbeConfidence::kNone;
    }
    const auto has_fcm = decoded->text.find("FCM:") != std::string::npos;
    const auto has_title = decoded->text.find("TITLE:") != std::string::npos;
    const auto has_timecode = decoded->text.find("00:00:") != std::string::npos;
    if (has_fcm && has_timecode) {
        return core::ProbeConfidence::kCertain;
    }
    if ((has_fcm || has_title) && has_timecode) {
        return core::ProbeConfidence::kHigh;
    }
    return has_fcm || has_title ? core::ProbeConfidence::kMedium
                                : core::ProbeConfidence::kNone;
}

core::ImportResult
Cmx3600Importer::Import(const core::ImportRequest &request) const {
    std::vector<Diagnostic> diagnostics;
    const auto decoded = DecodeText(request.content);
    if (!decoded.has_value()) {
        AddDiagnostic(diagnostics, DiagnosticSeverity::kError,
                      diagnostic_code::kInvalidEncoding,
                      "The input has an invalid UTF-16 encoding", request);
        return {.document = std::nullopt,
                .diagnostics = std::move(diagnostics)};
    }
    if (decoded->used_encoding_fallback) {
        AddDiagnostic(diagnostics, DiagnosticSeverity::kWarning,
                      diagnostic_code::kEncodingFallback,
                      "The input was decoded as Windows-1252", request);
    }

    std::vector<std::string_view> lines;
    std::string_view remaining = decoded->text;
    while (!remaining.empty()) {
        const auto newline = remaining.find('\n');
        auto line = remaining.substr(0, newline);
        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }
        lines.emplace_back(line);
        if (newline == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(newline + 1);
    }

    std::string title;
    TimecodeMode mode = TimecodeMode::kNonDropFrame;
    bool found_fcm = false;
    for (const auto line : lines) {
        const auto trimmed = Trim(line);
        if (trimmed.starts_with("TITLE:")) {
            title = std::string{Trim(trimmed.substr(6))};
        } else if (trimmed.starts_with("FCM:")) {
            found_fcm = true;
            const auto value = Trim(trimmed.substr(4));
            mode = value.find("DROP") != std::string_view::npos &&
                           value.find("NON-DROP") == std::string_view::npos
                       ? TimecodeMode::kDropFrame
                       : TimecodeMode::kNonDropFrame;
        }
    }

    const auto has_frame_rate_option = HasFrameRateOption(request);
    auto rate = ParseFrameRate(request);
    if (!rate.has_value() && !has_frame_rate_option &&
        mode == TimecodeMode::kDropFrame) {
        rate = FrameRate::Create(30'000, 1'001).value();
    }
    if (!rate.has_value()) {
        AddDiagnostic(diagnostics, DiagnosticSeverity::kError,
                      has_frame_rate_option
                          ? diagnostic_code::kInvalidFrameRate
                          : diagnostic_code::kMissingFrameRate,
                      has_frame_rate_option
                          ? "The cmx3600.frame_rate option is invalid"
                          : "A non-drop-frame CMX 3600 file requires the "
                            "cmx3600.frame_rate option",
                      request);
        return {.document = std::nullopt,
                .diagnostics = std::move(diagnostics)};
    }
    if (!Timecode::Create(0, 0, 0, 0, *rate, mode).has_value()) {
        AddDiagnostic(diagnostics, DiagnosticSeverity::kError,
                      diagnostic_code::kInvalidFrameRate,
                      "The selected frame rate is incompatible with the "
                      "file's timecode mode",
                      request);
        return {.document = std::nullopt,
                .diagnostics = std::move(diagnostics)};
    }

    core::TimelineDocument document{
        .title = std::move(title),
        .frame_rate = *rate,
        .timecode_mode = mode,
        .events = {},
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
    if (found_fcm) {
        document.metadata.emplace_back(
            "cmx3600.fcm",
            std::string{mode == TimecodeMode::kDropFrame ? "drop"
                                                         : "non-drop"});
    }

    core::EditEvent *last_event = nullptr;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto line_number = static_cast<std::uint64_t>(index + 1);
        const auto line = lines[index];
        const auto trimmed = Trim(line);
        if (trimmed.empty() || trimmed.starts_with("TITLE:") ||
            trimmed.starts_with("FCM:")) {
            continue;
        }
        if (trimmed.starts_with('*')) {
            if (last_event != nullptr) {
                AddComment(*last_event, request, line_number, line);
            } else {
                document.metadata.emplace_back(
                    "cmx3600.comment", std::string{Trim(trimmed.substr(1))});
            }
            continue;
        }
        if (trimmed.starts_with("M2 ")) {
            if (last_event == nullptr) {
                AddDiagnostic(diagnostics, DiagnosticSeverity::kWarning,
                              diagnostic_code::kOrphanRecord,
                              "Motion-effect record has no preceding event",
                              request, line_number);
            } else {
                last_event->metadata.emplace_back("cmx3600.motion_effect",
                                                  std::string{trimmed});
            }
            continue;
        }

        const auto tokens = Tokens(trimmed);
        if (tokens.empty() || !IsEventNumber(tokens.front())) {
            document.metadata.emplace_back("cmx3600.unknown_line",
                                           std::string{trimmed});
            AddDiagnostic(diagnostics, DiagnosticSeverity::kWarning,
                          diagnostic_code::kUnknownContent,
                          "Unrecognized CMX 3600 content was preserved",
                          request, line_number);
            continue;
        }
        if (tokens.size() < 8) {
            AddDiagnostic(diagnostics, DiagnosticSeverity::kError,
                          diagnostic_code::kMalformedEvent,
                          "Event record has too few fields", request,
                          line_number);
            continue;
        }

        const auto timecode_offset = tokens.size() - 4;
        std::array<std::optional<Timecode>, 4> timecodes;
        for (std::size_t timecode_index = 0; timecode_index < 4;
             ++timecode_index) {
            timecodes[timecode_index] = ParseTimecode(
                tokens[timecode_offset + timecode_index], *rate, mode);
        }
        if (std::ranges::any_of(timecodes, [](const auto &timecode) {
                return !timecode.has_value();
            })) {
            AddDiagnostic(diagnostics, DiagnosticSeverity::kError,
                          diagnostic_code::kInvalidTimecode,
                          "Event record contains an invalid timecode", request,
                          line_number);
            continue;
        }

        const auto source_range =
            core::TimecodeRange::Create(*timecodes[0], *timecodes[1]);
        const auto record_range =
            core::TimecodeRange::Create(*timecodes[2], *timecodes[3]);
        if (!source_range.has_value() || !record_range.has_value()) {
            AddDiagnostic(diagnostics, DiagnosticSeverity::kError,
                          diagnostic_code::kInvalidTimecode,
                          "Event record contains a reversed timecode range",
                          request, line_number);
            continue;
        }

        auto edit_type = core::EditType::kOther;
        std::optional<core::Transition> transition;
        const auto edit_code = tokens[3];
        if (edit_code == "C") {
            edit_type = core::EditType::kCut;
        } else if (edit_code == "D") {
            edit_type = core::EditType::kDissolve;
        } else if (edit_code.starts_with('W')) {
            edit_type = core::EditType::kWipe;
        } else if (edit_code.starts_with('K')) {
            edit_type = core::EditType::kKey;
        }

        if (edit_type != core::EditType::kCut) {
            std::uint64_t duration = 0;
            if (timecode_offset <= 4 ||
                !ParseInteger(tokens[timecode_offset - 1], duration)) {
                AddDiagnostic(
                    diagnostics, DiagnosticSeverity::kWarning,
                    diagnostic_code::kMalformedEvent,
                    "Transition record has no valid duration; zero was used",
                    request, line_number);
            }
            transition = core::Transition{
                .identifier = std::string{edit_code},
                .duration_frames = duration,
            };
        }

        document.events.emplace_back(core::EditEvent{
            .identifier = std::string{tokens[0]},
            .reel = std::string{tokens[1]},
            .track =
                {
                    .kind = TrackKind(tokens[2]),
                    .identifier = std::string{tokens[2]},
                },
            .edit_type = edit_type,
            .transition = std::move(transition),
            .source_range = *source_range,
            .record_range = *record_range,
            .comments = {},
            .metadata = {},
            .provenance = Provenance(request, line_number, line),
        });
        last_event = &document.events.back();
        if (edit_type == core::EditType::kOther) {
            last_event->metadata.emplace_back("cmx3600.edit_code",
                                              std::string{edit_code});
            AddDiagnostic(diagnostics, DiagnosticSeverity::kWarning,
                          diagnostic_code::kUnknownContent,
                          "Unknown edit type was preserved", request,
                          line_number);
        }
    }

    document.diagnostics = diagnostics;
    return core::ImportResult{
        .document = std::move(document),
        .diagnostics = std::move(diagnostics),
    };
}

} // namespace edit_atlas::formats::cmx3600
