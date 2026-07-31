#include <edit_atlas/app/application_menu_bar.hpp>
#include <edit_atlas/app/diagnostic_text.hpp>
#include <edit_atlas/app/document_view.hpp>
#include <edit_atlas/app/spreadsheet_export_options_dialog.hpp>
#include <edit_atlas/app/timeline_event_model.hpp>
#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace edit_atlas::app {
namespace {

TEST(ApplicationTest, LoadsBrazilianPortugueseTranslations) {
    QTranslator translator;

    ASSERT_TRUE(SetApplicationLanguage(
        translator, ApplicationLanguage::kBrazilianPortuguese));
    EXPECT_EQ(ApplicationMenuBar::tr("&File"), QStringLiteral("&Arquivo"));
    EXPECT_EQ(ApplicationMenuBar::tr("&Export Spreadsheet"),
              QStringLiteral("&Exportar planilha"));
    EXPECT_EQ(DocumentView::tr("No timeline open"),
              QStringLiteral("Nenhuma linha do tempo aberta"));
    EXPECT_EQ(SpreadsheetExportOptionsDialog::tr("Workbook language"),
              QStringLiteral("Idioma da pasta de trabalho"));
    EXPECT_EQ(SpreadsheetExportOptionsDialog::tr("Same as application"),
              QStringLiteral("Mesmo idioma do aplicativo"));
    EXPECT_EQ(ApplicationMenuBar::tr("Export Diagnostic &Logs"),
              QStringLiteral("Exportar &logs de diagnóstico"));

    const core::Diagnostic diagnostic{
        .severity = core::DiagnosticSeverity::kWarning,
        .code =
            std::string{formats::cmx3600::diagnostic_code::kMissingFrameRate},
        .message = "untranslated fallback",
        .location = std::nullopt,
    };
    EXPECT_EQ(diagnostic_text::Message(diagnostic),
              QStringLiteral("Uma taxa de quadros é necessária para esta EDL "
                             "non-drop-frame."));
}

TEST(ApplicationTest, PresentsTimelineDocumentInTableModel) {
    const auto rate = core::FrameRate::Create(24, 1);
    ASSERT_TRUE(rate.has_value());
    const auto source_start = core::Timecode::FromFrameCount(
        0, *rate, core::TimecodeMode::kNonDropFrame);
    const auto source_end = core::Timecode::FromFrameCount(
        24, *rate, core::TimecodeMode::kNonDropFrame);
    const auto record_start = core::Timecode::FromFrameCount(
        86'400, *rate, core::TimecodeMode::kNonDropFrame);
    const auto record_end = core::Timecode::FromFrameCount(
        86'424, *rate, core::TimecodeMode::kNonDropFrame);
    ASSERT_TRUE(source_start.has_value());
    ASSERT_TRUE(source_end.has_value());
    ASSERT_TRUE(record_start.has_value());
    ASSERT_TRUE(record_end.has_value());
    const auto source_range =
        core::TimecodeRange::Create(*source_start, *source_end);
    const auto record_range =
        core::TimecodeRange::Create(*record_start, *record_end);
    ASSERT_TRUE(source_range.has_value());
    ASSERT_TRUE(record_range.has_value());
    const core::TimelineDocument document{
        .title = "APP TEST",
        .frame_rate = *rate,
        .timecode_mode = core::TimecodeMode::kNonDropFrame,
        .events =
            {
                core::EditEvent{
                    .identifier = "001",
                    .reel = "AX",
                    .track =
                        {
                            .kind = core::TrackKind::kVideo,
                            .identifier = "V",
                        },
                    .edit_type = core::EditType::kCut,
                    .transition = std::nullopt,
                    .source_range = *source_range,
                    .record_range = *record_range,
                    .comments = {},
                    .metadata = {},
                    .provenance = std::nullopt,
                },
            },
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
    TimelineEventModel model;
    model.SetDocument(&document);

    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.columnCount(), 11);
    EXPECT_EQ(model.data(model.index(0, 0)).toString(), QStringLiteral("001"));
}

} // namespace
} // namespace edit_atlas::app
