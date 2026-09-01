#include <edit_atlas/presentation/timeline_document_workflow.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/timeline_document_export_service.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>

#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include <gtest/gtest.h>
#include <minizip/unzip.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace edit_atlas::presentation {
namespace {

[[nodiscard]] std::filesystem::path FilesystemPath(const QString &path) {
    const auto utf8 = path.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

[[nodiscard]] std::filesystem::path FixturePath(std::string_view name) {
    return std::filesystem::path{EDIT_ATLAS_INTEGRATION_FIXTURE_DIRECTORY} /
           name;
}

[[nodiscard]] bool WaitForSignal(QSignalSpy &spy) {
    return !spy.isEmpty() || spy.wait(5'000);
}

struct ImportGate final {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
};

class BlockingImporter final : public core::Importer {
  public:
    explicit BlockingImporter(std::shared_ptr<ImportGate> gate)
        : gate_{std::move(gate)} {}

    [[nodiscard]] const core::FormatDescriptor &
    descriptor(void) const noexcept override {
        return descriptor_;
    }

    [[nodiscard]] core::ProbeConfidence
    Probe(const core::ImportRequest &) const override {
        return core::ProbeConfidence::kCertain;
    }

    [[nodiscard]] core::ImportResult
    Import(const core::ImportRequest &) const override {
        std::unique_lock lock{gate_->mutex};
        gate_->entered = true;
        gate_->changed.notify_all();
        gate_->changed.wait(lock, [this](void) { return gate_->released; });
        return core::ImportResult{
            .document = std::nullopt,
            .diagnostics =
                {
                    core::Diagnostic{
                        .severity = core::DiagnosticSeverity::kError,
                        .code = "test.released",
                        .message = "The blocking importer was released.",
                        .location = std::nullopt,
                    },
                },
        };
    }

  private:
    std::shared_ptr<ImportGate> gate_;
    core::FormatDescriptor descriptor_{
        .identifier = "blocking",
        .display_name = "Blocking test format",
        .extensions = {"block"},
    };
};

[[nodiscard]] std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
}

[[nodiscard]] std::optional<std::string>
ReadZipEntry(const std::filesystem::path &path, std::string_view entry_name) {
    const auto path_text = path.string();
    auto *archive = unzOpen64(path_text.c_str());
    if (archive == nullptr) {
        return std::nullopt;
    }
    const std::string name{entry_name};
    if (unzLocateFile(archive, name.c_str(), 0) != UNZ_OK ||
        unzOpenCurrentFile(archive) != UNZ_OK) {
        static_cast<void>(unzClose(archive));
        return std::nullopt;
    }

    std::string content;
    std::vector<char> buffer(4'096);
    int bytes_read = 0;
    while ((bytes_read = unzReadCurrentFile(
                archive, buffer.data(),
                static_cast<unsigned int>(buffer.size()))) > 0) {
        content.append(buffer.data(), static_cast<std::size_t>(bytes_read));
    }
    const auto file_close = unzCloseCurrentFile(archive);
    const auto archive_close = unzClose(archive);
    if (bytes_read < 0 || file_close != UNZ_OK || archive_close != UNZ_OK) {
        return std::nullopt;
    }
    return content;
}

TEST(TimelineDocumentWorkflowTest, ReportsBusyUntilAsynchronousWorkFinishes) {
    auto gate = std::make_shared<ImportGate>();
    core::FormatRegistry registry;
    ASSERT_TRUE(
        registry.RegisterImporter(std::make_unique<BlockingImporter>(gate))
            .has_value());
    QTemporaryFile source{QStringLiteral("XXXXXX.block")};
    ASSERT_TRUE(source.open());
    source.write("blocking");
    source.close();
    TimelineDocumentWorkflow workflow{registry};
    QSignalSpy finished{&workflow, &TimelineDocumentWorkflow::importFinished};

    workflow.Import(services::TimelineDocumentImportRequest{
        .path = FilesystemPath(source.fileName()),
        .format_identifier = "blocking",
        .options = {},
    });
    bool entered = false;
    {
        std::unique_lock lock{gate->mutex};
        entered =
            gate->changed.wait_for(lock, std::chrono::seconds{5},
                                   [gate](void) { return gate->entered; });
        if (!entered) {
            gate->released = true;
        }
    }
    gate->changed.notify_all();
    ASSERT_TRUE(entered);
    EXPECT_TRUE(workflow.IsBusy());

    {
        std::lock_guard lock{gate->mutex};
        gate->released = true;
    }
    gate->changed.notify_all();
    ASSERT_TRUE(WaitForSignal(finished));
    EXPECT_FALSE(workflow.IsBusy());
    EXPECT_FALSE(workflow.ImportResult().has_value());
}

TEST(TimelineDocumentWorkflowTest,
     ImportsThenExportsARealLocalizedProjectedWorkbook) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    TimelineDocumentWorkflow workflow{*registry};
    QSignalSpy imported{&workflow, &TimelineDocumentWorkflow::importFinished};

    workflow.Import(services::TimelineDocumentImportRequest{
        .path = FixturePath("mixed_tracks.edl"),
        .format_identifier = {},
        .options =
            {
                core::MetadataEntry{
                    .key =
                        std::string{
                            formats::cmx3600::kFrameRateOption,
                        },
                    .value = "24",
                },
            },
    });
    ASSERT_TRUE(WaitForSignal(imported));
    auto import_result = workflow.ImportResult();
    ASSERT_TRUE(import_result.has_value());
    ASSERT_EQ(import_result->timeline.events.size(), 4);
    const auto filtered = services::FilterTimelineEvents(
        import_result->timeline,
        services::TimelineFilterQuery{
            .combination = services::TimelineFilterCombination::kAll,
            .conditions =
                {
                    services::TimelineTrackKindFilterCondition{
                        .track_kind = core::TrackKind::kAudio,
                    },
                },
        });
    ASSERT_TRUE(filtered.has_value());
    ASSERT_EQ(filtered->size(), 1);
    auto selected_timeline =
        services::SelectTimelineEvents(import_result->timeline, *filtered);
    ASSERT_EQ(selected_timeline.events.size(), 1);
    EXPECT_EQ(selected_timeline.events.front().identifier, "002");

    QTemporaryDir output;
    ASSERT_TRUE(output.isValid());
    const auto destination = FilesystemPath(output.path()) / "report.xlsx";
    QSignalSpy exported{&workflow, &TimelineDocumentWorkflow::exportFinished};
    workflow.Export(services::TimelineDocumentExportRequest{
        .path = destination,
        .format_identifier =
            std::string{
                formats::xlsx::kFormatIdentifier,
            },
        .timeline = std::move(selected_timeline),
        .event_projection =
            {
                core::TimelineEventField::kComments,
                core::TimelineEventField::kEventIdentifier,
                core::TimelineEventField::kDurationFrames,
            },
        .options =
            {
                core::MetadataEntry{
                    .key =
                        std::string{
                            formats::xlsx::kWorkbookLanguageOption,
                        },
                    .value = "pt-BR",
                },
            },
        .event_images = {},
        .replace_existing = false,
    });
    ASSERT_TRUE(WaitForSignal(exported));
    const auto export_result = workflow.ExportResult();
    ASSERT_TRUE(export_result.has_value());
    const auto content = ReadFile(destination);
    ASSERT_GE(content.size(), 2);
    EXPECT_EQ(content[0], 'P');
    EXPECT_EQ(content[1], 'K');
    const auto shared_strings =
        ReadZipEntry(destination, "xl/sharedStrings.xml");
    ASSERT_TRUE(shared_strings.has_value());
    EXPECT_NE(shared_strings->find("<t>Comentários</t>"), std::string::npos);
    EXPECT_NE(shared_strings->find("<t>Evento</t>"), std::string::npos);
    EXPECT_NE(shared_strings->find("<t>Duração em quadros</t>"),
              std::string::npos);
    EXPECT_NE(shared_strings->find("<t>002</t>"), std::string::npos);
    EXPECT_NE(shared_strings->find("SYNTHETIC AUDIO NOTE"), std::string::npos);
    EXPECT_EQ(shared_strings->find("opening.mov"), std::string::npos);
    EXPECT_EQ(shared_strings->find("<t>Rolo</t>"), std::string::npos);
}

TEST(TimelineDocumentWorkflowTest,
     PreservesImportDiagnosticsAcrossAsynchronousExecution) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    TimelineDocumentWorkflow workflow{*registry};
    QSignalSpy finished{&workflow, &TimelineDocumentWorkflow::importFinished};

    workflow.Import(services::TimelineDocumentImportRequest{
        .path = FixturePath("malformed.edl"),
        .format_identifier = {},
        .options =
            {
                core::MetadataEntry{
                    .key =
                        std::string{
                            formats::cmx3600::kFrameRateOption,
                        },
                    .value = "24",
                },
            },
    });
    ASSERT_TRUE(WaitForSignal(finished));
    const auto result = workflow.ImportResult();
    ASSERT_TRUE(result.has_value());

    const auto has_diagnostic = [&result](std::string_view code) {
        return std::ranges::any_of(result->diagnostics,
                                   [code](const core::Diagnostic &diagnostic) {
                                       return diagnostic.code == code;
                                   });
    };
    EXPECT_TRUE(
        has_diagnostic(formats::cmx3600::diagnostic_code::kMalformedEvent));
    EXPECT_TRUE(
        has_diagnostic(formats::cmx3600::diagnostic_code::kInvalidTimecode));
    EXPECT_TRUE(
        has_diagnostic(formats::cmx3600::diagnostic_code::kUnknownContent));
}

TEST(TimelineDocumentWorkflowTest, RecoversAfterAnExportFailure) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const services::TimelineDocumentImportService importer{*registry};
    const auto imported =
        importer.Import(services::TimelineDocumentImportRequest{
            .path = FixturePath("mixed_tracks.edl"),
            .format_identifier = {},
            .options =
                {
                    core::MetadataEntry{
                        .key =
                            std::string{
                                formats::cmx3600::kFrameRateOption,
                            },
                        .value = "24",
                    },
                },
        });
    ASSERT_TRUE(imported.has_value());

    QTemporaryDir output;
    ASSERT_TRUE(output.isValid());
    TimelineDocumentWorkflow workflow{*registry};
    QSignalSpy finished{&workflow, &TimelineDocumentWorkflow::exportFinished};
    workflow.Export(services::TimelineDocumentExportRequest{
        .path = FilesystemPath(output.path()) / "invalid.xlsx",
        .format_identifier = std::string{formats::xlsx::kFormatIdentifier},
        .timeline = imported->timeline,
        .event_projection = {},
        .options = {},
        .event_images = {},
        .replace_existing = false,
    });
    ASSERT_TRUE(WaitForSignal(finished));
    const auto failure = workflow.ExportResult();
    ASSERT_FALSE(failure.has_value());
    EXPECT_EQ(failure.error().kind,
              services::TimelineDocumentExportFailureKind::kInvalidRequest);

    finished.clear();
    const auto destination = FilesystemPath(output.path()) / "recovered.xlsx";
    workflow.Export(services::TimelineDocumentExportRequest{
        .path = destination,
        .format_identifier = std::string{formats::xlsx::kFormatIdentifier},
        .timeline = imported->timeline,
        .event_projection =
            {
                core::TimelineEventField::kEventIdentifier,
            },
        .options = {},
        .event_images = {},
        .replace_existing = false,
    });
    ASSERT_TRUE(WaitForSignal(finished));
    const auto recovered = workflow.ExportResult();
    ASSERT_TRUE(recovered.has_value());
    EXPECT_EQ(recovered->path, destination);
    EXPECT_TRUE(std::filesystem::exists(destination));
}

TEST(TimelineDocumentWorkflowTest, RecoversAfterAnImportFailure) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    TimelineDocumentWorkflow workflow{*registry};
    QSignalSpy finished{&workflow, &TimelineDocumentWorkflow::importFinished};

    workflow.Import(services::TimelineDocumentImportRequest{
        .path = FixturePath("missing.edl"),
        .format_identifier = {},
        .options = {},
    });
    ASSERT_TRUE(WaitForSignal(finished));
    ASSERT_FALSE(workflow.ImportResult().has_value());
    finished.clear();

    workflow.Import(services::TimelineDocumentImportRequest{
        .path = FixturePath("mixed_tracks.edl"),
        .format_identifier = {},
        .options =
            {
                core::MetadataEntry{
                    .key =
                        std::string{
                            formats::cmx3600::kFrameRateOption,
                        },
                    .value = "24",
                },
            },
    });
    ASSERT_TRUE(WaitForSignal(finished));
    const auto recovered = workflow.ImportResult();
    ASSERT_TRUE(recovered.has_value());
    EXPECT_EQ(recovered->timeline.events.size(), 4);
}

} // namespace
} // namespace edit_atlas::presentation
