#include <edit_atlas/app/application.hpp>
#include <edit_atlas/app/main_window.hpp>
#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

namespace edit_atlas::app {
namespace {

TEST(ApplicationTest, RegistersEveryBuiltInFormatHandler) {
    const auto registry = CreateFormatRegistry();

    ASSERT_TRUE(registry.has_value());
    EXPECT_NE(registry->FindImporter(formats::cmx3600::kFormatIdentifier),
              nullptr);
    EXPECT_NE(registry->FindExporter(formats::xlsx::kFormatIdentifier),
              nullptr);
    EXPECT_EQ(registry->importer_formats().size(), 1);
    EXPECT_EQ(registry->exporter_formats().size(), 1);
}

TEST(ApplicationTest, LoadsBrazilianPortugueseTranslations) {
    QTranslator translator;

    ASSERT_TRUE(SetApplicationLanguage(
        translator, ApplicationLanguage::kBrazilianPortuguese));
    EXPECT_EQ(MainWindow::tr("&File"), QStringLiteral("&Arquivo"));
}

} // namespace
} // namespace edit_atlas::app
