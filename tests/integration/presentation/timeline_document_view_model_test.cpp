#include <edit_atlas/presentation/timeline_document_view_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>
#include <edit_atlas/services/timeline_filter.hpp>

#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QVariant>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace edit_atlas::presentation {
namespace {

[[nodiscard]] std::filesystem::path FixturePath(std::string_view name) {
    return std::filesystem::path{EDIT_ATLAS_INTEGRATION_FIXTURE_DIRECTORY} /
           name;
}

[[nodiscard]] std::filesystem::path FilesystemPath(const QString &path) {
    const auto utf8 = path.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

[[nodiscard]] bool WaitForSignal(QSignalSpy &spy) {
    return !spy.isEmpty() || spy.wait(5'000);
}

[[nodiscard]] services::TimelineDocumentImportRequest ImportRequest(void) {
    return services::TimelineDocumentImportRequest{
        .path = FixturePath("mixed_tracks.edl"),
        .format_identifier = {},
        .options =
            {
                core::MetadataEntry{
                    .key = std::string{formats::cmx3600::kFrameRateOption},
                    .value = "24",
                },
            },
    };
}

TEST(TimelineDocumentViewModelTest, ImportsFiltersAndClearsTimelineState) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    TimelineDocumentViewModel view_model{*registry};
    QSignalSpy imported{&view_model,
                        &TimelineDocumentViewModel::DocumentStateChanged};

    ASSERT_TRUE(view_model.Import(ImportRequest()).has_value());
    EXPECT_EQ(view_model.DocumentState(), TimelineDocumentState::kImporting);
    EXPECT_TRUE(view_model.IsBusy());
    const auto clear_while_importing = view_model.Clear();
    ASSERT_FALSE(clear_while_importing.has_value());
    EXPECT_EQ(clear_while_importing.error(),
              TimelineDocumentCommandError::kBusy);
    imported.clear();
    ASSERT_TRUE(WaitForSignal(imported));

    ASSERT_EQ(view_model.DocumentState(), TimelineDocumentState::kReady);
    ASSERT_NE(view_model.Document(), nullptr);
    EXPECT_EQ(view_model.Document()->events.size(), 4);
    EXPECT_EQ(view_model.SourcePath(), FixturePath("mixed_tracks.edl"));
    EXPECT_EQ(view_model.EventSelection().size(), 4);
    EXPECT_EQ(view_model.EventModel().rowCount(), 4);
    EXPECT_TRUE(view_model.CanExport());

    view_model.SetFilterQuery(services::TimelineFilterQuery{
        .combination = services::TimelineFilterCombination::kAll,
        .conditions =
            {
                services::TimelineTrackKindFilterCondition{
                    .track_kind = core::TrackKind::kAudio,
                },
            },
    });
    ASSERT_EQ(view_model.EventSelection().size(), 1);
    EXPECT_EQ(view_model.EventModel().rowCount(), 1);
    EXPECT_EQ(view_model.EventModel()
                  .data(view_model.EventModel().index(0, 0))
                  .toString(),
              QStringLiteral("002"));

    view_model.SetFilterQuery(services::TimelineFilterQuery{
        .combination = services::TimelineFilterCombination::kAll,
        .conditions =
            {
                services::TimelineTextFilterCondition{
                    .field = services::TimelineTextFilterField::kClip,
                    .text = "*",
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = true,
                },
            },
    });
    EXPECT_NE(view_model.FilterError(), nullptr);
    EXPECT_TRUE(view_model.EventSelection().empty());
    EXPECT_EQ(view_model.EventModel().rowCount(), 0);
    EXPECT_FALSE(view_model.CanExport());

    ASSERT_TRUE(view_model.Clear().has_value());
    EXPECT_EQ(view_model.DocumentState(), TimelineDocumentState::kEmpty);
    EXPECT_EQ(view_model.Document(), nullptr);
    EXPECT_TRUE(view_model.SourcePath().empty());
}

TEST(TimelineDocumentViewModelTest, ExportsTheFilteredProjectedTimeline) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    TimelineDocumentViewModel view_model{*registry};
    QSignalSpy document_changed{&view_model,
                                &TimelineDocumentViewModel::DocumentChanged};
    ASSERT_TRUE(view_model.Import(ImportRequest()).has_value());
    document_changed.clear();
    ASSERT_TRUE(WaitForSignal(document_changed));
    ASSERT_EQ(view_model.DocumentState(), TimelineDocumentState::kReady);

    view_model.SetFilterQuery(services::TimelineFilterQuery{
        .combination = services::TimelineFilterCombination::kAll,
        .conditions =
            {
                services::TimelineTrackKindFilterCondition{
                    .track_kind = core::TrackKind::kAudio,
                },
            },
    });
    ASSERT_TRUE(
        view_model
            .SetEventProjection({core::TimelineEventField::kEventIdentifier})
            .has_value());

    QTemporaryDir output;
    ASSERT_TRUE(output.isValid());
    const auto destination = FilesystemPath(output.path()) / "filtered.xlsx";
    QSignalSpy exported{&view_model,
                        &TimelineDocumentViewModel::ExportFinished};
    ASSERT_TRUE(view_model
                    .Export(TimelineExportRequest{
                        .path = destination,
                        .format_identifier =
                            std::string{formats::xlsx::kFormatIdentifier},
                        .options = {},
                        .video_path = std::nullopt,
                        .replace_existing = false,
                    })
                    .has_value());
    EXPECT_EQ(view_model.ExportState(), TimelineExportState::kExporting);
    EXPECT_TRUE(view_model.IsBusy());
    ASSERT_TRUE(WaitForSignal(exported));

    EXPECT_EQ(view_model.ExportState(), TimelineExportState::kIdle);
    ASSERT_NE(view_model.ExportResult(), nullptr);
    ASSERT_TRUE(view_model.ExportResult()->has_value());
    EXPECT_EQ(view_model.ExportResult()->value().document_export.path,
              destination);
    EXPECT_TRUE(std::filesystem::exists(destination));
}

TEST(TimelineDocumentViewModelTest, RejectsCommandsWithInvalidState) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    TimelineDocumentViewModel view_model{*registry};

    const auto export_result = view_model.Export(TimelineExportRequest{
        .path = std::filesystem::path{"unused.xlsx"},
        .format_identifier = std::string{formats::xlsx::kFormatIdentifier},
        .options = {},
        .video_path = std::nullopt,
        .replace_existing = false,
    });
    ASSERT_FALSE(export_result.has_value());
    EXPECT_EQ(export_result.error(), TimelineDocumentCommandError::kNoDocument);

    const auto projection_result = view_model.SetEventProjection({});
    ASSERT_FALSE(projection_result.has_value());
    EXPECT_EQ(projection_result.error(),
              TimelineDocumentCommandError::kInvalidProjection);
    EXPECT_TRUE(
        core::IsValidTimelineEventProjection(view_model.EventProjection()));
}

TEST(TimelineDocumentViewModelTest, RetainsImportFailureForTheFrontend) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    TimelineDocumentViewModel view_model{*registry};
    QSignalSpy changed{&view_model,
                       &TimelineDocumentViewModel::DocumentChanged};
    const auto missing = FixturePath("missing.edl");

    ASSERT_TRUE(view_model
                    .Import(services::TimelineDocumentImportRequest{
                        .path = missing,
                        .format_identifier = {},
                        .options = {},
                    })
                    .has_value());
    changed.clear();
    ASSERT_TRUE(WaitForSignal(changed));

    EXPECT_EQ(view_model.DocumentState(), TimelineDocumentState::kImportFailed);
    EXPECT_EQ(view_model.Document(), nullptr);
    ASSERT_NE(view_model.ImportFailure(), nullptr);
    EXPECT_EQ(view_model.ImportFailure()->path, missing);
    EXPECT_EQ(view_model.ImportFailure()->kind,
              services::TimelineDocumentImportFailureKind::kOpenFailed);
}

} // namespace
} // namespace edit_atlas::presentation
