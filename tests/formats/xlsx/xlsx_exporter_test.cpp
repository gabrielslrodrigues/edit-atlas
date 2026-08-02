#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <gtest/gtest.h>
#include <minizip/unzip.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace edit_atlas::formats::xlsx {
namespace {

class TemporaryWorkbook final {
  public:
    explicit TemporaryWorkbook(const std::vector<std::byte> &content)
        : path_(std::filesystem::path{testing::TempDir()} /
                (std::string{testing::UnitTest::GetInstance()
                                 ->current_test_info()
                                 ->name()} +
                 ".xlsx")) {
        std::ofstream stream{path_, std::ios::binary | std::ios::trunc};
        stream.write(reinterpret_cast<const char *>(content.data()),
                     static_cast<std::streamsize>(content.size()));
    }

    ~TemporaryWorkbook(void) {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    TemporaryWorkbook(const TemporaryWorkbook &) = delete;
    TemporaryWorkbook &operator=(const TemporaryWorkbook &) = delete;
    TemporaryWorkbook(TemporaryWorkbook &&) = delete;
    TemporaryWorkbook &operator=(TemporaryWorkbook &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] std::optional<std::string>
ReadZipEntry(const std::filesystem::path &path, std::string_view entry_name) {
    const auto path_text = path.string();
    auto *archive = unzOpen64(path_text.c_str());
    if (archive == nullptr) {
        return std::nullopt;
    }

    const std::string name{entry_name};
    if (unzLocateFile(archive, name.c_str(), 0) != UNZ_OK) {
        unzClose(archive);
        return std::nullopt;
    }

    unz_file_info64 information{};
    if (unzGetCurrentFileInfo64(archive, &information, nullptr, 0, nullptr, 0,
                                nullptr, 0) != UNZ_OK ||
        unzOpenCurrentFile(archive) != UNZ_OK) {
        unzClose(archive);
        return std::nullopt;
    }

    std::string content(static_cast<std::size_t>(information.uncompressed_size),
                        '\0');
    std::size_t offset = 0;
    while (offset < content.size()) {
        const auto remaining = content.size() - offset;
        const auto chunk_size =
            remaining > static_cast<std::size_t>(0x7FFF'FFFF)
                ? 0x7FFF'FFFFU
                : static_cast<unsigned int>(remaining);
        const auto bytes_read =
            unzReadCurrentFile(archive, content.data() + offset, chunk_size);
        if (bytes_read <= 0) {
            unzCloseCurrentFile(archive);
            unzClose(archive);
            return std::nullopt;
        }
        offset += static_cast<std::size_t>(bytes_read);
    }

    const auto entry_close_result = unzCloseCurrentFile(archive);
    const auto archive_close_result = unzClose(archive);
    if (entry_close_result != UNZ_OK || archive_close_result != UNZ_OK) {
        return std::nullopt;
    }
    return content;
}

[[nodiscard]] std::optional<std::size_t>
SharedStringIndex(std::string_view shared_strings, std::string_view text) {
    const auto element = std::string{"<t>"} + std::string{text} + "</t>";
    const auto text_position = shared_strings.find(element);
    if (text_position == std::string_view::npos) {
        return std::nullopt;
    }

    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = shared_strings.find("<si>", position)) < text_position) {
        ++count;
        position += 4;
    }
    if (count == 0) {
        return std::nullopt;
    }
    return count - 1;
}

[[nodiscard]] bool CellUsesSharedString(std::string_view worksheet,
                                        std::string_view cell,
                                        std::size_t shared_string_index) {
    const auto reference = std::string{"r=\""} + std::string{cell} + "\"";
    const auto cell_position = worksheet.find(reference);
    if (cell_position == std::string_view::npos) {
        return false;
    }
    const auto cell_end = worksheet.find("</c>", cell_position);
    if (cell_end == std::string_view::npos) {
        return false;
    }
    const auto value =
        std::string{"<v>"} + std::to_string(shared_string_index) + "</v>";
    const auto value_position = worksheet.find(value, cell_position);
    return value_position != std::string_view::npos &&
           value_position < cell_end;
}

[[nodiscard]] core::Timecode TimecodeAt(std::int64_t frame_count,
                                        const core::FrameRate &rate) {
    return core::Timecode::FromFrameCount(frame_count, rate,
                                          core::TimecodeMode::kDropFrame)
        .value();
}

[[nodiscard]] core::TimecodeRange RangeAt(std::int64_t frame_count,
                                          std::int64_t duration,
                                          const core::FrameRate &rate) {
    return core::TimecodeRange::Create(TimecodeAt(frame_count, rate),
                                       TimecodeAt(frame_count + duration, rate))
        .value();
}

[[nodiscard]] core::TimelineDocument Document(void) {
    const auto rate = core::FrameRate::Create(30'000, 1'001).value();
    core::EditEvent event{
        .identifier = "001",
        .reel = "AX",
        .track =
            core::Track{
                .kind = core::TrackKind::kVideo,
                .identifier = "V",
            },
        .edit_type = core::EditType::kDissolve,
        .transition =
            core::Transition{
                .identifier = "D",
                .duration_frames = 12,
            },
        .source_range = RangeAt(17'982, 30, rate),
        .record_range = RangeAt(107'892, 30, rate),
        .comments =
            {
                core::Comment{
                    .text = "Opening shot",
                    .provenance = std::nullopt,
                },
            },
        .metadata =
            {
                core::MetadataEntry{
                    .key = "clip_name",
                    .value = std::string{"=SUM(A1:A2)"},
                },
                core::MetadataEntry{
                    .key = "source_file",
                    .value = std::string{"opening.mov"},
                },
            },
        .provenance =
            core::SourceLineProvenance{
                .location =
                    core::SourceLocation{
                        .source = "example.edl",
                        .line = 4,
                        .column = 1,
                    },
                .line = "001 AX V D ...",
            },
    };

    return core::TimelineDocument{
        .title = "Example Timeline",
        .frame_rate = rate,
        .timecode_mode = core::TimecodeMode::kDropFrame,
        .events = {std::move(event)},
        .metadata =
            {
                core::MetadataEntry{
                    .key = "source_format",
                    .value = std::string{"cmx-3600"},
                },
                core::MetadataEntry{
                    .key = "playback_speed",
                    .value = 1.25,
                },
                core::MetadataEntry{
                    .key = "interlaced",
                    .value = false,
                },
            },
        .diagnostics =
            {
                core::Diagnostic{
                    .severity = core::DiagnosticSeverity::kWarning,
                    .code = "cmx3600.unknown_content",
                    .message = "An unknown source line was preserved.",
                    .location =
                        core::SourceLocation{
                            .source = "example.edl",
                            .line = 8,
                            .column = 1,
                        },
                },
            },
        .provenance = std::nullopt,
    };
}

[[nodiscard]] core::ExportResult ExportDocument(WorkbookLanguage language) {
    const XlsxExporter exporter;
    const auto document = Document();
    return exporter.Export(core::ExportRequest{
        .document = document,
        .event_projection =
            {
                core::DefaultTimelineEventProjection().begin(),
                core::DefaultTimelineEventProjection().end(),
            },
        .options =
            {
                core::MetadataEntry{
                    .key = std::string{kWorkbookLanguageOption},
                    .value = std::string{WorkbookLanguageTag(language)},
                },
            },
    });
}

[[nodiscard]] core::ExportResult
ExportDocument(std::vector<core::TimelineEventField> projection) {
    const XlsxExporter exporter;
    const auto document = Document();
    return exporter.Export(core::ExportRequest{
        .document = document,
        .event_projection = std::move(projection),
        .options = {},
    });
}

[[nodiscard]] core::ExportResult ExportDocument(void) {
    return ExportDocument(WorkbookLanguage::kEnglish);
}

TEST(XlsxExporterTest, DescribesAndExportsTheFormat) {
    const XlsxExporter exporter;
    const auto result = ExportDocument();

    EXPECT_EQ(exporter.descriptor().identifier, kFormatIdentifier);
    EXPECT_EQ(exporter.descriptor().extensions,
              std::vector<std::string>{"xlsx"});
    ASSERT_TRUE(result.artifact.has_value());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_EQ(result.artifact->suggested_extension, "xlsx");
    EXPECT_EQ(
        result.artifact->media_type,
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
    ASSERT_GE(result.artifact->content.size(), 2);
    EXPECT_EQ(result.artifact->content[0], static_cast<std::byte>('P'));
    EXPECT_EQ(result.artifact->content[1], static_cast<std::byte>('K'));
}

TEST(XlsxExporterTest, WritesStableSheetsAndTimelineValues) {
    const auto result = ExportDocument();
    ASSERT_TRUE(result.artifact.has_value());
    const TemporaryWorkbook workbook{result.artifact->content};

    const auto workbook_xml = ReadZipEntry(workbook.path(), "xl/workbook.xml");
    const auto shared_strings =
        ReadZipEntry(workbook.path(), "xl/sharedStrings.xml");
    const auto timeline =
        ReadZipEntry(workbook.path(), "xl/worksheets/sheet2.xml");
    ASSERT_TRUE(workbook_xml.has_value());
    ASSERT_TRUE(shared_strings.has_value());
    ASSERT_TRUE(timeline.has_value());
    EXPECT_NE(workbook_xml->find("name=\"Events\""), std::string::npos);
    EXPECT_NE(workbook_xml->find("name=\"Timeline\""), std::string::npos);
    EXPECT_NE(workbook_xml->find("name=\"Diagnostics\""), std::string::npos);
    EXPECT_NE(shared_strings->find("Example Timeline"), std::string::npos);
    EXPECT_NE(shared_strings->find("30000/1001"), std::string::npos);
    EXPECT_NE(shared_strings->find("Drop Frame"), std::string::npos);
    EXPECT_NE(shared_strings->find("cmx-3600"), std::string::npos);
    EXPECT_NE(shared_strings->find("playback_speed"), std::string::npos);
    EXPECT_NE(timeline->find("<v>1</v>"), std::string::npos);
    EXPECT_NE(timeline->find("<v>1.25</v>"), std::string::npos);
}

TEST(XlsxExporterTest, LocalizesBrazilianPortuguesePresentation) {
    const auto result = ExportDocument(WorkbookLanguage::kBrazilianPortuguese);
    ASSERT_TRUE(result.artifact.has_value());
    const TemporaryWorkbook workbook{result.artifact->content};

    const auto workbook_xml = ReadZipEntry(workbook.path(), "xl/workbook.xml");
    const auto shared_strings =
        ReadZipEntry(workbook.path(), "xl/sharedStrings.xml");
    const auto properties = ReadZipEntry(workbook.path(), "docProps/core.xml");
    const auto timeline =
        ReadZipEntry(workbook.path(), "xl/worksheets/sheet2.xml");
    ASSERT_TRUE(workbook_xml.has_value());
    ASSERT_TRUE(shared_strings.has_value());
    ASSERT_TRUE(properties.has_value());
    ASSERT_TRUE(timeline.has_value());

    EXPECT_NE(workbook_xml->find("name=\"Eventos\""), std::string::npos);
    EXPECT_NE(workbook_xml->find("name=\"Linha do tempo\""), std::string::npos);
    EXPECT_NE(workbook_xml->find("name=\"Diagnósticos\""), std::string::npos);
    EXPECT_NE(shared_strings->find("Tipo de edição"), std::string::npos);
    EXPECT_NE(shared_strings->find("Dissolução"), std::string::npos);
    EXPECT_NE(shared_strings->find("Taxa de quadros"), std::string::npos);
    EXPECT_NE(shared_strings->find("Aviso"), std::string::npos);
    EXPECT_NE(properties->find("Relatório de linha do tempo editorial"),
              std::string::npos);
    EXPECT_NE(properties->find("Criado pelo Edit Atlas"), std::string::npos);

    EXPECT_NE(shared_strings->find("=SUM(A1:A2)"), std::string::npos);
    EXPECT_NE(shared_strings->find("Opening shot"), std::string::npos);
    EXPECT_NE(shared_strings->find("opening.mov"), std::string::npos);
    EXPECT_NE(shared_strings->find("Example Timeline"), std::string::npos);
    EXPECT_NE(shared_strings->find("cmx3600.unknown_content"),
              std::string::npos);
    EXPECT_NE(shared_strings->find("An unknown source line was preserved."),
              std::string::npos);
    EXPECT_NE(timeline->find("<v>0</v>"), std::string::npos);
    EXPECT_NE(timeline->find("<v>1.25</v>"), std::string::npos);
}

TEST(XlsxExporterTest, PreservesEventValuesAsLiteralText) {
    const auto result = ExportDocument();
    ASSERT_TRUE(result.artifact.has_value());
    const TemporaryWorkbook workbook{result.artifact->content};

    const auto events =
        ReadZipEntry(workbook.path(), "xl/worksheets/sheet1.xml");
    const auto shared_strings =
        ReadZipEntry(workbook.path(), "xl/sharedStrings.xml");
    ASSERT_TRUE(events.has_value());
    ASSERT_TRUE(shared_strings.has_value());
    EXPECT_NE(shared_strings->find("00:10:00;00"), std::string::npos);
    EXPECT_NE(shared_strings->find("01:00:00;00"), std::string::npos);
    EXPECT_NE(shared_strings->find("=SUM(A1:A2)"), std::string::npos);
    EXPECT_NE(shared_strings->find("Opening shot"), std::string::npos);
    EXPECT_NE(shared_strings->find("opening.mov"), std::string::npos);
    EXPECT_NE(events->find("<v>12</v>"), std::string::npos);
    EXPECT_EQ(events->find("<f>"), std::string::npos);
}

TEST(XlsxExporterTest, WritesOnlySelectedEventColumnsInSelectedOrder) {
    const auto result = ExportDocument({
        core::TimelineEventField::kComments,
        core::TimelineEventField::kEventIdentifier,
    });
    ASSERT_TRUE(result.artifact.has_value());
    const TemporaryWorkbook workbook{result.artifact->content};

    const auto events =
        ReadZipEntry(workbook.path(), "xl/worksheets/sheet1.xml");
    const auto shared_strings =
        ReadZipEntry(workbook.path(), "xl/sharedStrings.xml");
    ASSERT_TRUE(events.has_value());
    ASSERT_TRUE(shared_strings.has_value());

    const auto comments_header = SharedStringIndex(*shared_strings, "Comments");
    const auto event_header = SharedStringIndex(*shared_strings, "Event");
    ASSERT_TRUE(comments_header.has_value());
    ASSERT_TRUE(event_header.has_value());
    EXPECT_TRUE(CellUsesSharedString(*events, "A1", *comments_header));
    EXPECT_TRUE(CellUsesSharedString(*events, "B1", *event_header));
    EXPECT_EQ(shared_strings->find("<t>Reel</t>"), std::string::npos);
    EXPECT_EQ(shared_strings->find("<t>AX</t>"), std::string::npos);
    EXPECT_EQ(events->find("r=\"C1\""), std::string::npos);
}

TEST(XlsxExporterTest, RejectsAnEmptyEventProjection) {
    const auto result = ExportDocument(std::vector<core::TimelineEventField>{});

    EXPECT_FALSE(result.artifact.has_value());
    ASSERT_EQ(result.diagnostics.size(), 1);
    EXPECT_EQ(result.diagnostics.front().code,
              diagnostic_code::kInvalidEventProjection);
}

TEST(XlsxExporterTest, WritesDiagnosticDetails) {
    const auto result = ExportDocument();
    ASSERT_TRUE(result.artifact.has_value());
    const TemporaryWorkbook workbook{result.artifact->content};

    const auto shared_strings =
        ReadZipEntry(workbook.path(), "xl/sharedStrings.xml");
    ASSERT_TRUE(shared_strings.has_value());
    EXPECT_NE(shared_strings->find("Warning"), std::string::npos);
    EXPECT_NE(shared_strings->find("cmx3600.unknown_content"),
              std::string::npos);
    EXPECT_NE(shared_strings->find("An unknown source line was preserved."),
              std::string::npos);
    EXPECT_NE(shared_strings->find("example.edl"), std::string::npos);
}

TEST(XlsxExporterTest, OmitsOptionalSheetsWhenRequested) {
    const XlsxExporter exporter;
    const auto document = Document();
    const auto result = exporter.Export(core::ExportRequest{
        .document = document,
        .event_projection =
            {
                core::DefaultTimelineEventProjection().begin(),
                core::DefaultTimelineEventProjection().end(),
            },
        .options =
            {
                core::MetadataEntry{
                    .key = std::string{kIncludeTimelineSheetOption},
                    .value = false,
                },
                core::MetadataEntry{
                    .key = std::string{kIncludeDiagnosticsSheetOption},
                    .value = false,
                },
            },
    });
    ASSERT_TRUE(result.artifact.has_value());
    const TemporaryWorkbook workbook{result.artifact->content};

    const auto workbook_xml = ReadZipEntry(workbook.path(), "xl/workbook.xml");
    ASSERT_TRUE(workbook_xml.has_value());
    EXPECT_NE(workbook_xml->find("name=\"Events\""), std::string::npos);
    EXPECT_EQ(workbook_xml->find("name=\"Timeline\""), std::string::npos);
    EXPECT_EQ(workbook_xml->find("name=\"Diagnostics\""), std::string::npos);
}

} // namespace
} // namespace edit_atlas::formats::xlsx
