#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_document_pipeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include "format_test_doubles.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::core {
namespace {

[[nodiscard]] FormatDescriptor Descriptor(std::string identifier,
                                          std::string extension) {
    return FormatDescriptor{
        .identifier = std::move(identifier),
        .display_name = "Test Format",
        .extensions = {std::move(extension)},
    };
}

[[nodiscard]] TimelineDocument Document(void) {
    return TimelineDocument{
        .title = "Test",
        .frame_rate = FrameRate::Create(24, 1).value(),
        .timecode_mode = TimecodeMode::kNonDropFrame,
        .events = {},
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
}

[[nodiscard]] ImportResult SuccessfulImport(void) {
    return ImportResult{
        .document = Document(),
        .diagnostics = {},
    };
}

[[nodiscard]] ExportResult SuccessfulExport(void) {
    return ExportResult{
        .artifact =
            ExportArtifact{
                .content = {std::byte{0x4F}, std::byte{0x4B}},
                .suggested_extension = "txt",
                .media_type = "text/plain",
            },
        .diagnostics = {},
    };
}

[[nodiscard]] ImportRequest Request(std::string source_name = "input.bin",
                                    std::string extension = "") {
    static constexpr std::string_view kContent = "test content";
    const auto characters =
        std::span<const char>{kContent.data(), kContent.size()};
    return ImportRequest{
        .content = std::as_bytes(characters),
        .source_name = std::move(source_name),
        .extension = std::move(extension),
        .options = {},
    };
}

[[nodiscard]] bool HasDiagnostic(const std::vector<Diagnostic> &diagnostics,
                                 std::string_view code) {
    return std::ranges::any_of(diagnostics,
                               [code](const Diagnostic &diagnostic) {
                                   return diagnostic.code == code;
                               });
}

TEST(TimelineDocumentPipelineTest, UsesAnExplicitImporterWithoutProbing) {
    FormatRegistry registry;
    auto importer = std::make_unique<test::StubImporter>(
        Descriptor("cmx-3600", "edl"), ProbeConfidence::kNone);
    auto *importer_observer = importer.get();
    importer->set_result(SuccessfulImport());
    ASSERT_TRUE(registry.RegisterImporter(std::move(importer)));
    const TimelineDocumentPipeline pipeline{registry};

    const auto result =
        pipeline.Import(Request(), std::string_view{"cmx-3600"});

    EXPECT_TRUE(result.document.has_value());
    EXPECT_EQ(importer_observer->probe_count(), 0);
    EXPECT_EQ(importer_observer->import_count(), 1);
}

TEST(TimelineDocumentPipelineTest, SelectsTheStrongestContentProbe) {
    FormatRegistry registry;
    auto weak = std::make_unique<test::StubImporter>(Descriptor("weak", "one"),
                                                     ProbeConfidence::kLow);
    auto strong = std::make_unique<test::StubImporter>(
        Descriptor("strong", "two"), ProbeConfidence::kCertain);
    auto *strong_observer = strong.get();
    weak->set_result(SuccessfulImport());
    strong->set_result(SuccessfulImport());
    ASSERT_TRUE(registry.RegisterImporter(std::move(weak)));
    ASSERT_TRUE(registry.RegisterImporter(std::move(strong)));
    const TimelineDocumentPipeline pipeline{registry};

    const auto result = pipeline.Import(Request());

    EXPECT_TRUE(result.document.has_value());
    EXPECT_EQ(strong_observer->import_count(), 1);
}

TEST(TimelineDocumentPipelineTest, FallsBackToTheSourceFileExtension) {
    FormatRegistry registry;
    auto importer = std::make_unique<test::StubImporter>(
        Descriptor("cmx-3600", "edl"), ProbeConfidence::kNone);
    auto *importer_observer = importer.get();
    importer->set_result(SuccessfulImport());
    ASSERT_TRUE(registry.RegisterImporter(std::move(importer)));
    const TimelineDocumentPipeline pipeline{registry};

    const auto result = pipeline.Import(Request("CUT.EDL"));

    EXPECT_TRUE(result.document.has_value());
    EXPECT_EQ(importer_observer->import_count(), 1);
}

TEST(TimelineDocumentPipelineTest, UsesExtensionToBreakEqualProbeConfidence) {
    FormatRegistry registry;
    auto edl = std::make_unique<test::StubImporter>(
        Descriptor("cmx-3600", "edl"), ProbeConfidence::kHigh);
    auto xml = std::make_unique<test::StubImporter>(
        Descriptor("xml-timeline", "xml"), ProbeConfidence::kHigh);
    auto *edl_observer = edl.get();
    edl->set_result(SuccessfulImport());
    xml->set_result(SuccessfulImport());
    ASSERT_TRUE(registry.RegisterImporter(std::move(edl)));
    ASSERT_TRUE(registry.RegisterImporter(std::move(xml)));
    const TimelineDocumentPipeline pipeline{registry};

    const auto result = pipeline.Import(Request("cut.EDL"));

    EXPECT_TRUE(result.document.has_value());
    EXPECT_EQ(edl_observer->import_count(), 1);
}

TEST(TimelineDocumentPipelineTest, ReportsAmbiguousAutomaticDiscovery) {
    FormatRegistry registry;
    ASSERT_TRUE(registry.RegisterImporter(std::make_unique<test::StubImporter>(
        Descriptor("first", "one"), ProbeConfidence::kHigh)));
    ASSERT_TRUE(registry.RegisterImporter(std::make_unique<test::StubImporter>(
        Descriptor("second", "two"), ProbeConfidence::kHigh)));
    const TimelineDocumentPipeline pipeline{registry};

    const auto result = pipeline.Import(Request());

    EXPECT_FALSE(result.document.has_value());
    EXPECT_TRUE(HasDiagnostic(
        result.diagnostics, pipeline_diagnostic_code::kAmbiguousImportFormat));
}

TEST(TimelineDocumentPipelineTest, ReportsUnknownAutomaticAndExplicitFormats) {
    const FormatRegistry registry;
    const TimelineDocumentPipeline pipeline{registry};

    const auto automatic = pipeline.Import(Request());
    const auto explicit_format =
        pipeline.Import(Request(), std::string_view{"missing"});

    EXPECT_TRUE(HasDiagnostic(automatic.diagnostics,
                              pipeline_diagnostic_code::kUnknownImportFormat));
    EXPECT_TRUE(HasDiagnostic(explicit_format.diagnostics,
                              pipeline_diagnostic_code::kUnknownImportFormat));
}

TEST(TimelineDocumentPipelineTest,
     ContainsProbeExceptionsAndUsesExtensionFallback) {
    FormatRegistry registry;
    auto importer = std::make_unique<test::StubImporter>(
        Descriptor("cmx-3600", "edl"), ProbeConfidence::kCertain);
    importer->set_throw_during_probe(true);
    importer->set_result(SuccessfulImport());
    ASSERT_TRUE(registry.RegisterImporter(std::move(importer)));
    const TimelineDocumentPipeline pipeline{registry};

    const auto result = pipeline.Import(Request("cut.edl"));

    EXPECT_TRUE(result.document.has_value());
    EXPECT_TRUE(HasDiagnostic(result.diagnostics,
                              pipeline_diagnostic_code::kProbeException));
}

TEST(TimelineDocumentPipelineTest, ContainsImporterExceptions) {
    FormatRegistry registry;
    auto importer = std::make_unique<test::StubImporter>(
        Descriptor("cmx-3600", "edl"), ProbeConfidence::kCertain);
    importer->set_throw_during_import(true);
    ASSERT_TRUE(registry.RegisterImporter(std::move(importer)));
    const TimelineDocumentPipeline pipeline{registry};

    const auto result =
        pipeline.Import(Request(), std::string_view{"cmx-3600"});

    EXPECT_FALSE(result.document.has_value());
    EXPECT_TRUE(HasDiagnostic(result.diagnostics,
                              pipeline_diagnostic_code::kImportException));
}

TEST(TimelineDocumentPipelineTest,
     RejectsAnImporterResultWithoutDocumentOrError) {
    FormatRegistry registry;
    ASSERT_TRUE(registry.RegisterImporter(std::make_unique<test::StubImporter>(
        Descriptor("cmx-3600", "edl"), ProbeConfidence::kCertain)));
    const TimelineDocumentPipeline pipeline{registry};

    const auto result =
        pipeline.Import(Request(), std::string_view{"cmx-3600"});

    EXPECT_FALSE(result.document.has_value());
    EXPECT_TRUE(
        HasDiagnostic(result.diagnostics,
                      pipeline_diagnostic_code::kImportProducedNoDocument));
}

TEST(TimelineDocumentPipelineTest, ExportsWithARegisteredFormat) {
    FormatRegistry registry;
    auto exporter =
        std::make_unique<test::StubExporter>(Descriptor("xlsx", "xlsx"));
    auto *exporter_observer = exporter.get();
    exporter->set_result(SuccessfulExport());
    ASSERT_TRUE(registry.RegisterExporter(std::move(exporter)));
    const TimelineDocumentPipeline pipeline{registry};
    const auto document = Document();

    const auto result = pipeline.Export(
        ExportRequest{
            .document = document,
            .event_projection =
                {
                    DefaultTimelineEventProjection().begin(),
                    DefaultTimelineEventProjection().end(),
                },
            .options = {},
            .event_images = {},
        },
        "xlsx");

    ASSERT_TRUE(result.artifact.has_value());
    EXPECT_EQ(result.artifact->suggested_extension, "txt");
    EXPECT_EQ(exporter_observer->export_count(), 1);
}

TEST(TimelineDocumentPipelineTest, ReportsUnknownExportFormats) {
    const FormatRegistry registry;
    const TimelineDocumentPipeline pipeline{registry};
    const auto document = Document();

    const auto result = pipeline.Export(
        ExportRequest{
            .document = document,
            .event_projection =
                {
                    DefaultTimelineEventProjection().begin(),
                    DefaultTimelineEventProjection().end(),
                },
            .options = {},
            .event_images = {},
        },
        "missing");

    EXPECT_FALSE(result.artifact.has_value());
    EXPECT_TRUE(HasDiagnostic(result.diagnostics,
                              pipeline_diagnostic_code::kUnknownExportFormat));
}

TEST(TimelineDocumentPipelineTest, ContainsExporterExceptions) {
    FormatRegistry registry;
    auto exporter =
        std::make_unique<test::StubExporter>(Descriptor("xlsx", "xlsx"));
    exporter->set_throw_during_export(true);
    ASSERT_TRUE(registry.RegisterExporter(std::move(exporter)));
    const TimelineDocumentPipeline pipeline{registry};
    const auto document = Document();

    const auto result = pipeline.Export(
        ExportRequest{
            .document = document,
            .event_projection =
                {
                    DefaultTimelineEventProjection().begin(),
                    DefaultTimelineEventProjection().end(),
                },
            .options = {},
            .event_images = {},
        },
        "xlsx");

    EXPECT_FALSE(result.artifact.has_value());
    EXPECT_TRUE(HasDiagnostic(result.diagnostics,
                              pipeline_diagnostic_code::kExportException));
}

TEST(TimelineDocumentPipelineTest,
     RejectsAnExporterResultWithoutArtifactOrError) {
    FormatRegistry registry;
    ASSERT_TRUE(registry.RegisterExporter(
        std::make_unique<test::StubExporter>(Descriptor("xlsx", "xlsx"))));
    const TimelineDocumentPipeline pipeline{registry};
    const auto document = Document();

    const auto result = pipeline.Export(
        ExportRequest{
            .document = document,
            .event_projection =
                {
                    DefaultTimelineEventProjection().begin(),
                    DefaultTimelineEventProjection().end(),
                },
            .options = {},
            .event_images = {},
        },
        "xlsx");

    EXPECT_FALSE(result.artifact.has_value());
    EXPECT_TRUE(
        HasDiagnostic(result.diagnostics,
                      pipeline_diagnostic_code::kExportProducedNoArtifact));
}

} // namespace
} // namespace edit_atlas::core
