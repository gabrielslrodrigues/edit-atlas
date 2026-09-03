#include <edit_atlas/frontends/widgets/application_menu_bar.hpp>
#include <edit_atlas/frontends/widgets/application_style.hpp>

#include <edit_atlas/presentation/appearance.hpp>
#include <edit_atlas/frontends/widgets/spreadsheet_export_options_dialog.hpp>
#include <edit_atlas/frontends/widgets/timeline_document_view.hpp>
#include <edit_atlas/presentation/diagnostic_text.hpp>
#include <edit_atlas/presentation/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/services/timeline_video_inspection_service.hpp>

#include <QColor>
#include <QCoreApplication>
#include <QPalette>
#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace edit_atlas::frontends::widgets {
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
                  "edit_atlas::frontends::widgets::TimelineFilterWidget",
                  "Clear filters"),
              QStringLiteral("Limpar filtros"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::frontends::widgets::TimelineFilterWidget",
                  "Filter conditions"),
              QStringLiteral("Condições de filtro"));
    EXPECT_EQ(
        QCoreApplication::translate(
            "edit_atlas::frontends::widgets::TimelineFilterWidget", "Filters"),
        QStringLiteral("Filtros"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::frontends::widgets::TimelineFilterWidget",
                  "Use regular expression"),
              QStringLiteral("Usar expressão regular"));
    EXPECT_EQ(
        QCoreApplication::translate(
            "edit_atlas::presentation::TimelineFilterModel", "Track type"),
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
                  "edit_atlas::frontends::widgets::EventProjectionWidget",
                  "Event columns"),
              QStringLiteral("Colunas de eventos"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::frontends::widgets::EventProjectionWidget",
                  "Duration"),
              QStringLiteral("Duração"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::frontends::widgets::EventProjectionWidget",
                  "Duration frames"),
              QStringLiteral("Duração em quadros"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::frontends::widgets::EventProjectionWidget",
                  "Initial frame"),
              QStringLiteral("Quadro inicial"));
    EXPECT_EQ(
        QCoreApplication::translate(
            "edit_atlas::frontends::widgets::EventProjectionWidget", "Move up"),
        QStringLiteral("Mover para cima"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::frontends::widgets::TimelineFilterWidget",
                  "No template"),
              QStringLiteral("Nenhum modelo"));
    EXPECT_EQ(
        QCoreApplication::translate(
            "edit_atlas::frontends::widgets::TimelineFilterWidget", "Modified"),
        QStringLiteral("Modificado"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::frontends::widgets::TimelineFilterWidget",
                  "Template actions"),
              QStringLiteral("Ações do modelo"));
    EXPECT_EQ(QCoreApplication::translate(
                  "edit_atlas::frontends::widgets::TimelineTemplateController",
                  "Save Filter and Export Template"),
              QStringLiteral("Salvar modelo de filtro e exportação"));
    EXPECT_EQ(ApplicationMenuBar::tr("Export Diagnostic &Logs"),
              QStringLiteral("Exportar &logs de diagnóstico"));
    EXPECT_EQ(ApplicationMenuBar::tr("&Appearance"),
              QStringLiteral("A&parência"));
    EXPECT_EQ(ApplicationMenuBar::tr("&System"), QStringLiteral("&Sistema"));
    EXPECT_EQ(ApplicationMenuBar::tr("&Light"), QStringLiteral("&Claro"));
    EXPECT_EQ(ApplicationMenuBar::tr("&Dark"), QStringLiteral("&Escuro"));

    // The Qt Quick frontend declares the same menu in QML, under the context
    // lupdate names after the file, and shares this catalogue.
    EXPECT_EQ(QCoreApplication::translate("Main", "&Appearance"),
              QStringLiteral("A&parência"));
    EXPECT_EQ(QCoreApplication::translate("Main", "&System"),
              QStringLiteral("&Sistema"));
    EXPECT_EQ(QCoreApplication::translate("Main", "&Light"),
              QStringLiteral("&Claro"));
    EXPECT_EQ(QCoreApplication::translate("Main", "&Dark"),
              QStringLiteral("&Escuro"));

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

TEST(ApplicationStyleTest, ResolvesEveryStyleSheetTokenFromThePalette) {
    for (const auto resolved : {presentation::ResolvedAppearance::kLight,
                                presentation::ResolvedAppearance::kDark}) {
        const auto &palette = presentation::AppearancePaletteFor(resolved);
        const auto style_sheet = LoadApplicationStyleSheet(palette);
        ASSERT_FALSE(style_sheet.isEmpty());
        EXPECT_TRUE(style_sheet.contains(QStringLiteral("QMainWindow")));

        // An unresolved token would render as a literal, so the stylesheet
        // must name no token the palette did not supply.
        EXPECT_FALSE(style_sheet.contains(QLatin1Char('@')))
            << style_sheet.toStdString();
        EXPECT_TRUE(style_sheet.contains(palette.window));
        EXPECT_TRUE(style_sheet.contains(palette.border));
    }
}

TEST(ApplicationStyleTest, StyleSheetsDifferBetweenAppearances) {
    const auto light = LoadApplicationStyleSheet(
        presentation::AppearancePaletteFor(
            presentation::ResolvedAppearance::kLight));
    const auto dark = LoadApplicationStyleSheet(
        presentation::AppearancePaletteFor(
            presentation::ResolvedAppearance::kDark));
    EXPECT_NE(light, dark);
}

TEST(ApplicationStyleTest, BuildsAWidgetPaletteFromTheSharedPalette) {
    const auto &shared = presentation::AppearancePaletteFor(
        presentation::ResolvedAppearance::kDark);
    const auto palette = BuildApplicationPalette(shared);
    EXPECT_EQ(palette.color(QPalette::Window), QColor{shared.window});
    EXPECT_EQ(palette.color(QPalette::Highlight), QColor{shared.accent});
    EXPECT_EQ(palette.color(QPalette::Disabled, QPalette::Text),
              QColor{shared.disabled});
}

} // namespace
} // namespace edit_atlas::frontends::widgets
