#include <edit_atlas/presentation/timeline_template_model.hpp>

#include <edit_atlas/services/timeline_template.hpp>

#include <QString>

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <string_view>

namespace edit_atlas::presentation {
namespace {

TEST(TimelineTemplateModelTest, PresentsNoTemplateAndStableSavedChoices) {
    const std::array templates{
        services::TimelineTemplate{
            .identifier = "first-id",
            .name = "First",
            .filter = {},
            .event_projection = {},
        },
        services::TimelineTemplate{
            .identifier = "second-id",
            .name = "Second",
            .filter = {},
            .event_projection = {},
        },
    };
    TimelineTemplateModel model;

    model.SetTemplates(templates, std::string_view{"second-id"}, true);

    ASSERT_EQ(model.rowCount(), 3);
    EXPECT_EQ(model.data(model.index(0, 0)).toString(),
              QStringLiteral("No template"));
    EXPECT_TRUE(model
                    .data(model.index(0, 0),
                          TimelineTemplateModel::kIdentifierRole)
                    .toString()
                    .isEmpty());
    EXPECT_EQ(model.data(model.index(1, 0)).toString(),
              QStringLiteral("First"));
    EXPECT_EQ(model
                  .data(model.index(2, 0),
                        TimelineTemplateModel::kIdentifierRole)
                  .toString(),
              QStringLiteral("second-id"));
    EXPECT_TRUE(model
                    .data(model.index(2, 0),
                          TimelineTemplateModel::kActiveRole)
                    .toBool());
    EXPECT_TRUE(model
                    .data(model.index(2, 0),
                          TimelineTemplateModel::kModifiedRole)
                    .toBool());
    EXPECT_EQ(model.ActiveRow(), 2);
}

TEST(TimelineTemplateModelTest, FallsBackToTheNoTemplateChoice) {
    TimelineTemplateModel model;

    model.SetTemplates({}, std::nullopt, true);

    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.ActiveRow(), 0);
    EXPECT_TRUE(model
                    .data(model.index(0, 0),
                          TimelineTemplateModel::kActiveRole)
                    .toBool());
    EXPECT_FALSE(model
                     .data(model.index(0, 0),
                           TimelineTemplateModel::kModifiedRole)
                     .toBool());
}

} // namespace
} // namespace edit_atlas::presentation
