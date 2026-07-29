#include <edit_atlas/app/application.hpp>
#include <edit_atlas/app/document_loader.hpp>
#include <edit_atlas/app/main_window.hpp>
#include <edit_atlas/app/timeline_event_model.hpp>
#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <QByteArray>
#include <QDir>
#include <QString>
#include <QTemporaryFile>
#include <QTranslator>
#include <QVariant>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

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

TEST(ApplicationTest, LoadsCmx3600DocumentFromLocalFile) {
    auto registry = CreateFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    QTemporaryFile file{QDir::tempPath() +
                        QStringLiteral("/edit-atlas-XXXXXX.edl")};
    ASSERT_TRUE(file.open());
    const QByteArray content{"TITLE: APP TEST\n"
                             "FCM: NON-DROP FRAME\n"
                             "001 AX V C 00:00:00:00 00:00:01:00 "
                             "01:00:00:00 01:00:01:00\n"};
    ASSERT_EQ(file.write(content), content.size());
    file.close();

    const auto result =
        LoadDocument(*registry, file.fileName(), std::string{"24"});

    EXPECT_EQ(result.error, DocumentLoadError::kNone);
    ASSERT_TRUE(result.import_result.document.has_value());
    EXPECT_EQ(result.import_result.document->title, "APP TEST");
    EXPECT_EQ(result.import_result.document->events.size(), 1);

    TimelineEventModel model;
    model.SetDocument(&*result.import_result.document);
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.columnCount(), 11);
    EXPECT_EQ(model.data(model.index(0, 0)).toString(), QStringLiteral("001"));
}

TEST(ApplicationTest, ReportsMissingFrameRateForNonDropDocument) {
    auto registry = CreateFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    QTemporaryFile file{QDir::tempPath() +
                        QStringLiteral("/edit-atlas-XXXXXX.edl")};
    ASSERT_TRUE(file.open());
    const QByteArray content{"TITLE: APP TEST\n"
                             "FCM: NON-DROP FRAME\n"
                             "001 AX V C 00:00:00:00 00:00:01:00 "
                             "01:00:00:00 01:00:01:00\n"};
    ASSERT_EQ(file.write(content), content.size());
    file.close();

    const auto result = LoadDocument(*registry, file.fileName());

    EXPECT_EQ(result.error, DocumentLoadError::kNone);
    EXPECT_FALSE(result.import_result.document.has_value());
    EXPECT_TRUE(std::ranges::any_of(
        result.import_result.diagnostics,
        [](const core::Diagnostic &diagnostic) {
            return diagnostic.code ==
                   formats::cmx3600::diagnostic_code::kMissingFrameRate;
        }));
}

} // namespace
} // namespace edit_atlas::app
