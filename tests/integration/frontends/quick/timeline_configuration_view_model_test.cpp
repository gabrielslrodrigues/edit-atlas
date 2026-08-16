#include <edit_atlas/frontends/quick/timeline_configuration_view_model.hpp>

#include <edit_atlas/presentation/application_state.hpp>
#include <edit_atlas/presentation/timeline_document_view_model.hpp>
#include <edit_atlas/presentation/timeline_filter_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format_registry.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/timeline_document_import_service.hpp>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>
#include <QtGlobal>

#include <gtest/gtest.h>

#include <filesystem>
#include <functional>
#include <string>
#include <utility>

namespace edit_atlas::frontends::quick {
namespace {

[[nodiscard]] core::FormatRegistry BuiltInRegistry(void) {
    return services::CreateBuiltInFormatRegistry().value();
}

[[nodiscard]] std::filesystem::path FixturePath(void) {
    return std::filesystem::path{EDIT_ATLAS_QUICK_FIXTURE_DIRECTORY} /
           "mixed_tracks.edl";
}

[[nodiscard]] bool WaitUntil(const std::function<bool(void)> &condition,
                             int timeout_milliseconds = 5'000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout_milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return condition();
}

class TimelineConfigurationViewModelTest : public ::testing::Test {
  protected:
    void SetUp(void) override {
        static_cast<void>(std::filesystem::remove_all(
            presentation::ConfiguredTemplateDirectory()));
    }
};

TEST_F(TimelineConfigurationViewModelTest,
       ExposesInitialEditableConfiguration) {
    auto registry = BuiltInRegistry();
    presentation::TimelineDocumentViewModel document{registry};
    TimelineConfigurationViewModel configuration{
        document, presentation::ConfiguredTemplateDirectory()};

    EXPECT_EQ(configuration.FilterModel()->rowCount(), 1);
    EXPECT_EQ(configuration.TemplateModel()->rowCount(), 1);
    EXPECT_EQ(configuration.EventProjectionModel()->rowCount(),
              static_cast<int>(core::kTimelineEventFieldCount));
    EXPECT_EQ(configuration.ActiveTemplateRow(), 0);
    EXPECT_FALSE(configuration.HasActiveTemplate());
    EXPECT_FALSE(configuration.IsTemplateModified());
    EXPECT_TRUE(configuration.IsFilterValid());
    EXPECT_TRUE(configuration.FilterErrorText().isEmpty());
    EXPECT_TRUE(configuration.IsEventProjectionValid());
    EXPECT_EQ(configuration.EventProjectionSelectedCount(),
              static_cast<int>(core::kTimelineEventFieldCount - 1));
    EXPECT_NE(configuration.TextFilterEditor(),
              configuration.TrackKindFilterEditor());
    EXPECT_NE(configuration.TrackKindFilterEditor(),
              configuration.EditTypeFilterEditor());
    EXPECT_EQ(configuration.FilterCombinationNames().size(), 2);
    EXPECT_EQ(
        configuration.FilterFieldNames().size(),
        static_cast<qsizetype>(presentation::TimelineFilterField::kCount));
}

TEST_F(TimelineConfigurationViewModelTest,
       SynchronizesFilteringTemplatesAndEventProjection) {
    auto registry = BuiltInRegistry();
    presentation::TimelineDocumentViewModel document{registry};
    TimelineConfigurationViewModel configuration{
        document, presentation::ConfiguredTemplateDirectory()};
    services::TimelineDocumentImportRequest request{
        .path = FixturePath(),
        .format_identifier = {},
        .options =
            {
                core::MetadataEntry{
                    .key = std::string{formats::cmx3600::kFrameRateOption},
                    .value = std::string{"24"},
                },
            },
    };
    ASSERT_TRUE(document.Import(std::move(request)).has_value());
    configuration.RestoreForTimeline();
    ASSERT_TRUE(WaitUntil([&document] {
        return document.DocumentState() ==
               presentation::TimelineDocumentState::kReady;
    }));

    const auto filter_index = configuration.FilterModel()->index(0, 0);
    ASSERT_TRUE(configuration.FilterModel()->setData(
        filter_index, QStringLiteral("001"),
        presentation::TimelineFilterModel::kTextRole));
    EXPECT_EQ(document.EventSelection().size(), 1U);
    ASSERT_TRUE(configuration.CreateTemplate(QStringLiteral("First event")));
    EXPECT_EQ(configuration.TemplateModel()->rowCount(), 2);
    EXPECT_EQ(configuration.ActiveTemplateRow(), 1);
    EXPECT_EQ(configuration.ActiveTemplateName(),
              QStringLiteral("First event"));
    EXPECT_TRUE(configuration.HasActiveTemplate());
    EXPECT_FALSE(configuration.IsTemplateModified());
    EXPECT_FALSE(configuration.CreateTemplate(QStringLiteral("First event")));
    EXPECT_FALSE(configuration.TemplateOperationErrorText().isEmpty());
    configuration.ClearTemplateOperationError();
    EXPECT_TRUE(configuration.TemplateOperationErrorText().isEmpty());

    configuration.ClearFilter();
    EXPECT_EQ(document.EventSelection().size(), 4U);
    EXPECT_TRUE(configuration.IsTemplateModified());
    configuration.SelectTemplateRow(1);
    EXPECT_EQ(document.EventSelection().size(), 1U);
    EXPECT_FALSE(configuration.IsTemplateModified());

    configuration.SetEventProjectionSelected(0, false);
    EXPECT_TRUE(configuration.IsEventProjectionValid());
    EXPECT_EQ(configuration.EventProjectionSelectedCount(),
              static_cast<int>(core::kTimelineEventFieldCount - 2));
    EXPECT_TRUE(configuration.IsTemplateModified());
    EXPECT_TRUE(configuration.UpdateActiveTemplate());
    EXPECT_FALSE(configuration.IsTemplateModified());

    const auto restored_filter_index = configuration.FilterModel()->index(0, 0);
    ASSERT_TRUE(configuration.FilterModel()->setData(
        restored_filter_index, QStringLiteral("*"),
        presentation::TimelineFilterModel::kTextRole));
    ASSERT_TRUE(configuration.FilterModel()->setData(
        restored_filter_index, true,
        presentation::TimelineFilterModel::kRegularExpressionRole));
    EXPECT_FALSE(configuration.IsFilterValid());
    EXPECT_FALSE(configuration.FilterErrorText().isEmpty());
}

} // namespace
} // namespace edit_atlas::frontends::quick
