#include <edit_atlas/frontends/quick/spreadsheet_export_view_model.hpp>

#include <edit_atlas/presentation/timeline_document_view_model.hpp>

#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>

#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <string>

namespace edit_atlas::frontends::quick {
namespace {

[[nodiscard]] std::filesystem::path FixturePath(void) {
    return std::filesystem::path{EDIT_ATLAS_QUICK_FIXTURE_DIRECTORY} /
           "mixed_tracks.edl";
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
        .path = FixturePath(),
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

TEST(SpreadsheetExportViewModelTest,
     ExportsTheActiveProjectionWithWorkbookOptions) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    presentation::TimelineDocumentViewModel document{*registry};
    SpreadsheetExportViewModel export_view_model{*registry, document};
    EXPECT_FALSE(export_view_model.IsAvailable());

    QSignalSpy document_changed{
        &document, &presentation::TimelineDocumentViewModel::documentChanged};
    ASSERT_TRUE(document.Import(ImportRequest()).has_value());
    document_changed.clear();
    ASSERT_TRUE(WaitForSignal(document_changed));
    ASSERT_EQ(document.DocumentState(),
              presentation::TimelineDocumentState::kReady);
    EXPECT_TRUE(export_view_model.IsAvailable());
    EXPECT_FALSE(export_view_model.IsRenderedVideoRequired());
    EXPECT_TRUE(export_view_model.SuggestedDestinationUrl().fileName().endsWith(
        QStringLiteral("-report.xlsx")));

    QTemporaryDir output;
    ASSERT_TRUE(output.isValid());
    const auto destination = output.filePath(QStringLiteral("report.xlsx"));
    QSignalSpy succeeded{&export_view_model,
                         &SpreadsheetExportViewModel::exportSucceeded};
    ASSERT_TRUE(export_view_model.Start(QUrl::fromLocalFile(destination),
                                        QStringLiteral("pt-BR"), true, false,
                                        QUrl{}, false));
    EXPECT_TRUE(export_view_model.IsBusy());
    ASSERT_TRUE(WaitForSignal(succeeded));

    EXPECT_FALSE(export_view_model.IsBusy());
    EXPECT_EQ(export_view_model.ResultPath(), destination);
    EXPECT_TRUE(export_view_model.ResultDetailsText().isEmpty());
    EXPECT_FALSE(export_view_model.HasWarnings());
    EXPECT_TRUE(export_view_model.ErrorText().isEmpty());
    EXPECT_TRUE(std::filesystem::exists(FilesystemPath(destination)));
}

TEST(SpreadsheetExportViewModelTest,
     RequiresALocalRenderedVideoForTheInitialFrameColumn) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    presentation::TimelineDocumentViewModel document{*registry};
    SpreadsheetExportViewModel export_view_model{*registry, document};
    QSignalSpy document_changed{
        &document, &presentation::TimelineDocumentViewModel::documentChanged};
    ASSERT_TRUE(document.Import(ImportRequest()).has_value());
    document_changed.clear();
    ASSERT_TRUE(WaitForSignal(document_changed));
    ASSERT_TRUE(
        document.SetEventProjection({core::TimelineEventField::kInitialFrame})
            .has_value());
    ASSERT_TRUE(export_view_model.IsRenderedVideoRequired());

    QTemporaryDir output;
    ASSERT_TRUE(output.isValid());
    QSignalSpy failed{&export_view_model,
                      &SpreadsheetExportViewModel::exportFailed};
    EXPECT_FALSE(export_view_model.Start(
        QUrl::fromLocalFile(output.filePath(QStringLiteral("report.xlsx"))),
        QStringLiteral("en"), true, true, QUrl{}, false));

    EXPECT_EQ(failed.count(), 1);
    EXPECT_FALSE(export_view_model.ErrorText().isEmpty());
    EXPECT_FALSE(export_view_model.IsBusy());
}

} // namespace
} // namespace edit_atlas::frontends::quick
