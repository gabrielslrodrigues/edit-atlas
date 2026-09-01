#include <edit_atlas/presentation/timeline_filter_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/services/timeline_filter.hpp>

#include <QObject>
#include <QString>
#include <QtGlobal>

#include <gtest/gtest.h>

#include <cstdint>
#include <variant>

namespace edit_atlas::presentation {
namespace {

TEST(TimelineFilterModelTest, EditsAStablePresentationIndependentQuery) {
    TimelineFilterModel model;
    auto query_change_count = 0;
    QObject::connect(&model, &TimelineFilterModel::queryChanged,
                     [&query_change_count] { ++query_change_count; });

    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.FieldNames().size(),
              static_cast<qsizetype>(TimelineFilterField::kCount));
    EXPECT_EQ(model.CombinationNames().size(), 2);
    EXPECT_EQ(model.TrackKindNames().size(), 4);
    EXPECT_EQ(model.EditTypeNames().size(), 5);

    const auto first = model.index(0, 0);
    EXPECT_TRUE(model.setData(
        first, static_cast<int>(TimelineFilterField::kClip),
        TimelineFilterModel::kFieldRole));
    EXPECT_TRUE(model.setData(first, QStringLiteral("Opening"),
                              TimelineFilterModel::kTextRole));
    EXPECT_TRUE(model.setData(first, true,
                              TimelineFilterModel::kMatchCaseRole));
    EXPECT_TRUE(model.setData(first, true,
                              TimelineFilterModel::kMatchWholeWordRole));
    model.SetCombination(
        static_cast<int>(services::TimelineFilterCombination::kAny));

    const auto query = model.Query();
    EXPECT_EQ(query.combination, services::TimelineFilterCombination::kAny);
    ASSERT_EQ(query.conditions.size(), 1U);
    const auto &condition =
        std::get<services::TimelineTextFilterCondition>(query.conditions[0]);
    EXPECT_EQ(condition.field, services::TimelineTextFilterField::kClip);
    EXPECT_EQ(condition.text, "Opening");
    EXPECT_TRUE(condition.match_case);
    EXPECT_TRUE(condition.match_whole_word);
    EXPECT_FALSE(condition.regular_expression);
    EXPECT_EQ(query_change_count, 4);
}

TEST(TimelineFilterModelTest, RoundTripsEveryTypedCondition) {
    const services::TimelineFilterQuery query{
        .combination = services::TimelineFilterCombination::kAll,
        .conditions =
            {
                services::TimelineTextFilterCondition{
                    .field =
                        services::TimelineTextFilterField::kTrackIdentifier,
                    .text = "V2",
                    .match_case = true,
                    .match_whole_word = false,
                    .regular_expression = true,
                },
                services::TimelineTrackKindFilterCondition{
                    .track_kind = core::TrackKind::kAudio,
                },
                services::TimelineEditTypeFilterCondition{
                    .edit_type = core::EditType::kDissolve,
                },
                services::TimelineTimecodeFilterCondition{
                    .field = services::TimelineTimecodeFilterField::kRecordIn,
                    .timecode = "01:02:03:04",
                },
                services::TimelineDurationFilterCondition{
                    .frames = std::int64_t{48},
                },
            },
    };
    TimelineFilterModel model;

    model.SetQuery(query);

    EXPECT_EQ(model.rowCount(), 5);
    EXPECT_EQ(model.Query(), query);
    EXPECT_EQ(model.data(model.index(1, 0), TimelineFilterModel::kEditorRole)
                  .toInt(),
              static_cast<int>(TimelineFilterEditor::kTrackKind));
    EXPECT_EQ(model.data(model.index(2, 0), TimelineFilterModel::kSelectionRole)
                  .toInt(),
              static_cast<int>(core::EditType::kDissolve));
    EXPECT_EQ(model.data(model.index(3, 0), TimelineFilterModel::kTextRole)
                  .toString(),
              QStringLiteral("01:02:03:04"));
    EXPECT_EQ(model.data(model.index(4, 0), TimelineFilterModel::kTextRole)
                  .toString(),
              QStringLiteral("48"));
}

TEST(TimelineFilterModelTest, MaintainsOneEditableRowWhenCleared) {
    TimelineFilterModel model;
    model.AddCondition();
    ASSERT_EQ(model.rowCount(), 2);

    model.RemoveCondition(0);
    EXPECT_EQ(model.rowCount(), 1);
    model.RemoveCondition(0);
    EXPECT_EQ(model.rowCount(), 1);

    EXPECT_TRUE(model.setData(model.index(0, 0), QStringLiteral("temporary"),
                              TimelineFilterModel::kTextRole));
    model.Clear();

    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.Query(), services::TimelineFilterQuery{});
}

} // namespace
} // namespace edit_atlas::presentation
