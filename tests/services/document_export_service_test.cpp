#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/document_export_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace edit_atlas::services {
namespace {

class TemporaryDestination final {
  public:
    explicit TemporaryDestination(std::string filename)
        : path_(std::filesystem::path{testing::TempDir()} /
                std::move(filename)) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove(path_, error));
    }

    ~TemporaryDestination(void) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove(path_, error));
    }

    TemporaryDestination(const TemporaryDestination &) = delete;
    TemporaryDestination &operator=(const TemporaryDestination &) = delete;
    TemporaryDestination(TemporaryDestination &&) = delete;
    TemporaryDestination &operator=(TemporaryDestination &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] core::TimelineDocument Document(void) {
    const auto rate = core::FrameRate::Create(24, 1).value();
    return core::TimelineDocument{
        .title = "SERVICE EXPORT TEST",
        .frame_rate = rate,
        .timecode_mode = core::TimecodeMode::kNonDropFrame,
        .events = {},
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
}

void WriteFile(const std::filesystem::path &path, std::string_view content) {
    std::ofstream output{
        path,
        std::ios::binary | std::ios::out | std::ios::trunc,
    };
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

TEST(DocumentExportServiceTest, ExportsDocumentAtomically) {
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const DocumentExportService service{*registry};
    const TemporaryDestination destination{"service-export.xlsx"};

    const auto result = service.ExportDocument(ExportDocumentRequest{
        .path = destination.path(),
        .format_identifier = std::string{formats::xlsx::kFormatIdentifier},
        .document = Document(),
        .event_projection =
            {
                core::DefaultTimelineEventProjection().begin(),
                core::DefaultTimelineEventProjection().end(),
            },
        .options = {},
        .replace_existing = false,
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, destination.path());
    EXPECT_TRUE(result->diagnostics.empty());
    const auto content = ReadFile(destination.path());
    ASSERT_GE(content.size(), 2);
    EXPECT_EQ(content[0], 'P');
    EXPECT_EQ(content[1], 'K');
}

TEST(DocumentExportServiceTest, PreservesExistingFileWithoutPermission) {
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const DocumentExportService service{*registry};
    const TemporaryDestination destination{"service-existing.xlsx"};
    WriteFile(destination.path(), "original");

    const auto result = service.ExportDocument(ExportDocumentRequest{
        .path = destination.path(),
        .format_identifier = std::string{formats::xlsx::kFormatIdentifier},
        .document = Document(),
        .event_projection =
            {
                core::DefaultTimelineEventProjection().begin(),
                core::DefaultTimelineEventProjection().end(),
            },
        .options = {},
        .replace_existing = false,
    });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind,
              DocumentExportFailureKind::kDestinationExists);
    EXPECT_EQ(ReadFile(destination.path()), "original");
}

TEST(DocumentExportServiceTest, AtomicallyReplacesExistingFileWhenPermitted) {
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const DocumentExportService service{*registry};
    const TemporaryDestination destination{"service-replace.xlsx"};
    WriteFile(destination.path(), "original");

    const auto result = service.ExportDocument(ExportDocumentRequest{
        .path = destination.path(),
        .format_identifier = std::string{formats::xlsx::kFormatIdentifier},
        .document = Document(),
        .event_projection =
            {
                core::DefaultTimelineEventProjection().begin(),
                core::DefaultTimelineEventProjection().end(),
            },
        .options = {},
        .replace_existing = true,
    });

    ASSERT_TRUE(result.has_value());
    const auto content = ReadFile(destination.path());
    ASSERT_GE(content.size(), 2);
    EXPECT_EQ(content[0], 'P');
    EXPECT_EQ(content[1], 'K');
}

TEST(DocumentExportServiceTest, ReturnsExporterDiagnostics) {
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const DocumentExportService service{*registry};
    const TemporaryDestination destination{"service-unknown.xlsx"};

    const auto result = service.ExportDocument(ExportDocumentRequest{
        .path = destination.path(),
        .format_identifier = "unknown",
        .document = Document(),
        .event_projection =
            {
                core::DefaultTimelineEventProjection().begin(),
                core::DefaultTimelineEventProjection().end(),
            },
        .options = {},
        .replace_existing = false,
    });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, DocumentExportFailureKind::kExportFailed);
    EXPECT_FALSE(result.error().diagnostics.empty());
    EXPECT_FALSE(std::filesystem::exists(destination.path()));
}

TEST(DocumentExportServiceTest, RejectsAnEmptyEventProjection) {
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const DocumentExportService service{*registry};
    const TemporaryDestination destination{"service-empty-projection.xlsx"};

    const auto result = service.ExportDocument(ExportDocumentRequest{
        .path = destination.path(),
        .format_identifier = std::string{formats::xlsx::kFormatIdentifier},
        .document = Document(),
        .event_projection = {},
        .options = {},
        .replace_existing = false,
    });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, DocumentExportFailureKind::kInvalidRequest);
    EXPECT_FALSE(std::filesystem::exists(destination.path()));
}

} // namespace
} // namespace edit_atlas::services
