#include <edit_atlas/app/application_menu_bar.hpp>
#include <edit_atlas/app/application_style.hpp>
#include <edit_atlas/app/spreadsheet_export_options_dialog.hpp>
#include <edit_atlas/app/timeline_document_view.hpp>
#include <edit_atlas/presentation/diagnostic_text.hpp>
#include <edit_atlas/presentation/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <QCoreApplication>
#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace edit_atlas::app {
namespace {

TEST(ApplicationTest, LoadsBrazilianPortugueseTranslations) {
    QTranslator translator;

    ASSERT_TRUE(presentation::SetApplicationLanguage(
        translator, presentation::ApplicationLanguage::kBrazilianPortuguese));
    EXPECT_EQ(ApplicationMenuBar::tr("&File"), QStringLiteral("&Arquivo"));
    EXPECT_EQ(ApplicationMenuBar::tr("&Export Spreadsheet"),
              QStringLiteral("&Exportar planilha"));
    EXPECT_EQ(TimelineDocumentView::tr("No timeline open"),
              QStringLiteral("Nenhuma linha do tempo aberta"));
    EXPECT_EQ(TimelineDocumentView::tr("Showing %1 of %2 events"),
              QStringLiteral("Exibindo %1 de %2 eventos"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::TimelineFilterWidget", "Clear filters"),
              QStringLiteral("Limpar filtros"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::TimelineFilterWidget", "Filter conditions"),
              QStringLiteral("Condições de filtro"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::TimelineFilterWidget", "Filters"),
              QStringLiteral("Filtros"));
    EXPECT_EQ(
        QCoreApplication::translate("edit_atlas::app::TimelineFilterWidget",
                                    "Use regular expression"),
        QStringLiteral("Usar expressão regular"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::TimelineFilterWidget", "Track type"),
              QStringLiteral("Tipo de faixa"));
    EXPECT_EQ(SpreadsheetExportOptionsDialog::tr("Workbook language"),
              QStringLiteral("Idioma da pasta de trabalho"));
    EXPECT_EQ(SpreadsheetExportOptionsDialog::tr("Same as application"),
              QStringLiteral("Mesmo idioma do aplicativo"));
    EXPECT_EQ(SpreadsheetExportOptionsDialog::tr("Rendered video"),
              QStringLiteral("Vídeo renderizado"));
    EXPECT_EQ(SpreadsheetExportOptionsDialog::tr("Rendered video path"),
              QStringLiteral("Caminho do vídeo renderizado"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::EventProjectionWidget", "Event columns"),
              QStringLiteral("Colunas de eventos"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::EventProjectionWidget", "Duration"),
              QStringLiteral("Duração"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::EventProjectionWidget", "Duration frames"),
              QStringLiteral("Duração em quadros"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::EventProjectionWidget", "Initial frame"),
              QStringLiteral("Quadro inicial"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::EventProjectionWidget", "Move up"),
              QStringLiteral("Mover para cima"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::TimelineFilterWidget", "No template"),
              QStringLiteral("Nenhum modelo"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::TimelineFilterWidget", "Modified"),
              QStringLiteral("Modificado"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::TimelineFilterWidget", "Template actions"),
              QStringLiteral("Ações do modelo"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::app::TimelineTemplateController",
                  "Save Filter and Export Template"),
              QStringLiteral("Salvar modelo de filtro e exportação"));
    EXPECT_EQ(ApplicationMenuBar::tr("Export Diagnostic &Logs"),
              QStringLiteral("Exportar &logs de diagnóstico"));

    const core::Diagnostic diagnostic{
        .severity = core::DiagnosticSeverity::kWarning,
        .code =
            std::string{formats::cmx3600::diagnostic_code::kMissingFrameRate},
        .message = "untranslated fallback",
        .location = std::nullopt,
    };
    EXPECT_EQ(presentation::diagnostic_text::Message(diagnostic),
              QStringLiteral("Uma taxa de quadros é necessária para esta EDL "
                             "non-drop-frame."));
    const core::Diagnostic video_diagnostic{
        .severity = core::DiagnosticSeverity::kError,
        .code =
            std::string{
                services::timeline_video_diagnostic_code::kMissingTimecode},
        .message = "untranslated fallback",
        .location = std::nullopt,
    };
    EXPECT_EQ(presentation::diagnostic_text::Message(video_diagnostic),
              QStringLiteral("O vídeo renderizado não possui timecode inicial "
                             "incorporado legível."));
}

TEST(ApplicationStyleTest, LoadsEmbeddedStyleSheet) {
    const auto style_sheet = LoadApplicationStyleSheet();
    ASSERT_FALSE(style_sheet.isEmpty());
    EXPECT_TRUE(style_sheet.contains(QStringLiteral("QMainWindow")));
}

} // namespace
} // namespace edit_atlas::app
