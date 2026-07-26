#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::formats::cmx3600 {
namespace {

[[nodiscard]] std::string ReadFixture(std::string_view name) {
    const auto path =
        std::string{EDIT_ATLAS_CMX3600_FIXTURE_DIR} + "/" + std::string{name};
    std::ifstream stream{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
}

[[nodiscard]] std::span<const std::byte> Bytes(const std::string &text) {
    return std::as_bytes(std::span<const char>{text.data(), text.size()});
}

[[nodiscard]] core::ImportRequest
Request(const std::string &text,
        std::vector<core::MetadataEntry> options = {}) {
    return core::ImportRequest{
        .content = Bytes(text),
        .source_name = "synthetic.edl",
        .extension = "edl",
        .options = std::move(options),
    };
}

[[nodiscard]] core::MetadataEntry FrameRateOption(std::string value) {
    return core::MetadataEntry{
        .key = std::string{kFrameRateOption},
        .value = std::move(value),
    };
}

[[nodiscard]] bool
HasDiagnostic(const std::vector<core::Diagnostic> &diagnostics,
              std::string_view code) {
    return std::ranges::any_of(diagnostics,
                               [code](const core::Diagnostic &diagnostic) {
                                   return diagnostic.code == code;
                               });
}

[[nodiscard]] bool HasMetadata(const std::vector<core::MetadataEntry> &metadata,
                               std::string_view key) {
    return std::ranges::any_of(
        metadata,
        [key](const core::MetadataEntry &entry) { return entry.key == key; });
}

[[nodiscard]] std::string Utf16Le(std::string_view ascii) {
    std::string encoded;
    encoded.push_back(static_cast<char>(0xFF));
    encoded.push_back(static_cast<char>(0xFE));
    for (const auto character : ascii) {
        encoded.push_back(character);
        encoded.push_back('\0');
    }
    return encoded;
}

TEST(Cmx3600ImporterTest, DescribesAndRecognizesTheFormat) {
    const Cmx3600Importer importer;
    const auto text = ReadFixture("mixed_tracks.edl");

    EXPECT_EQ(importer.descriptor().identifier, kFormatIdentifier);
    EXPECT_EQ(importer.descriptor().extensions,
              std::vector<std::string>{"edl"});
    EXPECT_EQ(importer.Probe(Request(text)), core::ProbeConfidence::kCertain);
}

TEST(Cmx3600ImporterTest, ImportsMixedTracksTransitionsAndComments) {
    const Cmx3600Importer importer;
    const auto text = ReadFixture("mixed_tracks.edl");

    const auto result = importer.Import(Request(text, {FrameRateOption("24")}));

    ASSERT_TRUE(result.document.has_value());
    const auto &document = *result.document;
    EXPECT_EQ(document.title, "SYNTHETIC MIXED TRACKS");
    EXPECT_EQ(document.frame_rate, core::FrameRate::Create(24, 1).value());
    ASSERT_EQ(document.events.size(), 4);
    EXPECT_EQ(document.events[0].track.kind, core::TrackKind::kVideo);
    EXPECT_EQ(document.events[1].track.kind, core::TrackKind::kAudio);
    EXPECT_EQ(document.events[1].edit_type, core::EditType::kDissolve);
    ASSERT_TRUE(document.events[1].transition.has_value());
    EXPECT_EQ(document.events[1].transition->duration_frames, 12);
    EXPECT_EQ(document.events[2].edit_type, core::EditType::kWipe);
    EXPECT_EQ(document.events[3].edit_type, core::EditType::kKey);
    EXPECT_TRUE(HasMetadata(document.events[0].metadata, "clip_name"));
    EXPECT_TRUE(HasMetadata(document.events[0].metadata, "source_file"));
    EXPECT_TRUE(
        HasMetadata(document.events[1].metadata, "cmx3600.motion_effect"));
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(Cmx3600ImporterTest, SupportsCrLfUtf8BomAndDropFrame) {
    const Cmx3600Importer importer;
    const std::string text = "\xEF\xBB\xBF"
                             "TITLE: DROP TEST\r\n"
                             "FCM: DROP FRAME\r\n"
                             "001 AX V C 00:00:59;29 00:01:00;02 "
                             "01:00:00;00 01:00:00;01\r\n";

    const auto result = importer.Import(Request(text));

    ASSERT_TRUE(result.document.has_value());
    EXPECT_EQ(result.document->timecode_mode, core::TimecodeMode::kDropFrame);
    EXPECT_EQ(result.document->frame_rate,
              core::FrameRate::Create(30'000, 1'001).value());
    ASSERT_EQ(result.document->events.size(), 1);
    EXPECT_EQ(result.document->events.front().source_range.DurationInFrames(),
              1);
}

TEST(Cmx3600ImporterTest, SupportsUtf16LittleEndianInput) {
    const Cmx3600Importer importer;
    const auto text = Utf16Le("TITLE: UTF16\nFCM: NON-DROP FRAME\n"
                              "001 AX V C 00:00:00:00 00:00:01:00 "
                              "01:00:00:00 01:00:01:00\n");

    const auto result = importer.Import(Request(text, {FrameRateOption("25")}));

    ASSERT_TRUE(result.document.has_value());
    EXPECT_EQ(result.document->title, "UTF16");
    EXPECT_EQ(result.document->events.size(), 1);
}

TEST(Cmx3600ImporterTest, ReportsWindows1252Fallback) {
    const Cmx3600Importer importer;
    std::string text = "TITLE: WINDOWS 1252 \nFCM: NON-DROP FRAME\n"
                       "001 AX V C 00:00:00:00 00:00:01:00 "
                       "01:00:00:00 01:00:01:00\n";
    text.insert(text.find('\n') - 1, 1, static_cast<char>(0x96));

    const auto result = importer.Import(Request(text, {FrameRateOption("24")}));

    ASSERT_TRUE(result.document.has_value());
    EXPECT_TRUE(
        HasDiagnostic(result.diagnostics, diagnostic_code::kEncodingFallback));
}

TEST(Cmx3600ImporterTest, RequiresRateForNonDropFrameInput) {
    const Cmx3600Importer importer;
    const auto text = ReadFixture("mixed_tracks.edl");

    const auto result = importer.Import(Request(text));

    EXPECT_FALSE(result.document.has_value());
    EXPECT_TRUE(
        HasDiagnostic(result.diagnostics, diagnostic_code::kMissingFrameRate));
}

TEST(Cmx3600ImporterTest, ReportsMalformedAndUnknownLinesWithLocations) {
    const Cmx3600Importer importer;
    const auto text = ReadFixture("malformed.edl");

    const auto result = importer.Import(Request(text, {FrameRateOption("24")}));

    ASSERT_TRUE(result.document.has_value());
    EXPECT_TRUE(
        HasDiagnostic(result.diagnostics, diagnostic_code::kMalformedEvent));
    EXPECT_TRUE(
        HasDiagnostic(result.diagnostics, diagnostic_code::kInvalidTimecode));
    EXPECT_TRUE(
        HasDiagnostic(result.diagnostics, diagnostic_code::kUnknownContent));
    const auto malformed =
        std::ranges::find(result.diagnostics, diagnostic_code::kMalformedEvent,
                          &core::Diagnostic::code);
    ASSERT_NE(malformed, result.diagnostics.end());
    ASSERT_TRUE(malformed->location.has_value());
    EXPECT_EQ(malformed->location->line, 4);
    EXPECT_TRUE(HasMetadata(result.document->metadata, "cmx3600.unknown_line"));
}

TEST(Cmx3600ImporterTest, RejectsMalformedUtf16) {
    const Cmx3600Importer importer;
    const std::string text{
        static_cast<char>(0xFF),
        static_cast<char>(0xFE),
        'X',
    };

    const auto result = importer.Import(Request(text));

    EXPECT_FALSE(result.document.has_value());
    EXPECT_TRUE(
        HasDiagnostic(result.diagnostics, diagnostic_code::kInvalidEncoding));
}

} // namespace
} // namespace edit_atlas::formats::cmx3600
