#include <edit_atlas/presentation/timeline_template_view_model.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_template.hpp>

#include <QSignalSpy>
#include <QString>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

namespace edit_atlas::presentation {
namespace {

class TemporaryTemplateDirectory final {
  public:
    TemporaryTemplateDirectory(void)
        : path_{std::filesystem::path{testing::TempDir()} /
                ("edit-atlas-template-view-model-" +
                 services::GenerateTimelineTemplateIdentifier())} {
        std::filesystem::create_directories(path_);
    }

    ~TemporaryTemplateDirectory(void) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove_all(path_, error));
    }

    TemporaryTemplateDirectory(const TemporaryTemplateDirectory &) = delete;
    TemporaryTemplateDirectory &
    operator=(const TemporaryTemplateDirectory &) = delete;
    TemporaryTemplateDirectory(TemporaryTemplateDirectory &&) = delete;
    TemporaryTemplateDirectory &
    operator=(TemporaryTemplateDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] services::TimelineFilterQuery AudioFilter(void) {
    return services::TimelineFilterQuery{
        .combination = services::TimelineFilterCombination::kAll,
        .conditions =
            {
                services::TimelineTrackKindFilterCondition{
                    .track_kind = core::TrackKind::kAudio,
                },
            },
    };
}

[[nodiscard]] TimelineTemplateCommandError
CommandError(const TimelineTemplateCommandResult &result) {
    return std::get<TimelineTemplateCommandError>(result.error());
}

TEST(TimelineTemplateViewModelTest, CreatesUpdatesAndRestoresTemplateState) {
    const TemporaryTemplateDirectory directory;
    TimelineTemplateViewModel view_model{directory.path()};
    ASSERT_TRUE(view_model.Load().has_value());
    QSignalSpy modified{&view_model,
                        &TimelineTemplateViewModel::modifiedChanged};

    const auto filter = AudioFilter();
    view_model.SetFilterState(filter, true);
    ASSERT_TRUE(
        view_model
            .SetEventProjection({core::TimelineEventField::kEventIdentifier,
                                 core::TimelineEventField::kClipName})
            .has_value());
    ASSERT_TRUE(view_model.Create("Audio review").has_value());

    ASSERT_EQ(view_model.Templates().size(), 1);
    ASSERT_TRUE(view_model.ActiveIdentifier().has_value());
    ASSERT_NE(view_model.ActiveTemplate(), nullptr);
    EXPECT_EQ(view_model.ActiveTemplate()->name, "Audio review");
    EXPECT_FALSE(view_model.IsModified());
    ASSERT_EQ(view_model.TemplateModel().rowCount(), 2);
    EXPECT_EQ(view_model.TemplateModel().ActiveRow(), 1);
    EXPECT_EQ(view_model.TemplateModel()
                  .data(view_model.TemplateModel().index(1, 0))
                  .toString(),
              QStringLiteral("Audio review"));

    view_model.SetFilterState({}, true);
    EXPECT_TRUE(view_model.IsModified());
    EXPECT_TRUE(view_model.TemplateModel()
                    .data(view_model.TemplateModel().index(1, 0),
                          TimelineTemplateModel::kModifiedRole)
                    .toBool());
    ASSERT_TRUE(view_model.UpdateActive().has_value());
    EXPECT_FALSE(view_model.IsModified());
    EXPECT_GE(modified.count(), 2);

    view_model.SetFilterState(filter, true);
    ASSERT_TRUE(
        view_model.SetEventProjection({core::TimelineEventField::kRecordIn})
            .has_value());
    view_model.RestoreForTimeline();
    EXPECT_TRUE(view_model.FilterQuery().conditions.empty());
    EXPECT_TRUE(std::ranges::equal(
        view_model.EventProjection(),
        std::vector{core::TimelineEventField::kEventIdentifier,
                    core::TimelineEventField::kClipName}));
    EXPECT_FALSE(view_model.IsModified());
}

TEST(TimelineTemplateViewModelTest, ManagesTheActiveTemplateCatalog) {
    const TemporaryTemplateDirectory directory;
    TimelineTemplateViewModel view_model{directory.path()};
    ASSERT_TRUE(view_model.Load().has_value());
    view_model.SetFilterState(AudioFilter(), true);
    ASSERT_TRUE(view_model.Create("Original").has_value());
    const auto original_identifier = *view_model.ActiveIdentifier();

    ASSERT_TRUE(view_model.RenameActive("Renamed").has_value());
    ASSERT_NE(view_model.ActiveTemplate(), nullptr);
    EXPECT_EQ(view_model.ActiveTemplate()->name, "Renamed");
    ASSERT_TRUE(view_model.DuplicateActive("Copy").has_value());
    ASSERT_EQ(view_model.Templates().size(), 2);
    ASSERT_NE(view_model.ActiveTemplate(), nullptr);
    EXPECT_EQ(view_model.ActiveTemplate()->name, "Copy");
    EXPECT_NE(*view_model.ActiveIdentifier(), original_identifier);

    ASSERT_TRUE(view_model.RemoveActive().has_value());
    EXPECT_EQ(view_model.Templates().size(), 1);
    EXPECT_FALSE(view_model.ActiveIdentifier().has_value());
    EXPECT_EQ(view_model.ActiveTemplate(), nullptr);
    EXPECT_TRUE(view_model.FilterQuery().conditions.empty());

    ASSERT_TRUE(view_model.SelectTemplate(original_identifier));
    EXPECT_EQ(view_model.ActiveTemplate()->name, "Renamed");
    view_model.SelectNoTemplate();
    EXPECT_FALSE(view_model.ActiveIdentifier().has_value());
    EXPECT_TRUE(view_model.FilterQuery().conditions.empty());
}

TEST(TimelineTemplateViewModelTest, ReloadsPersistedTemplateState) {
    const TemporaryTemplateDirectory directory;
    TimelineTemplateViewModel original{directory.path()};
    ASSERT_TRUE(original.Load().has_value());
    original.SetFilterState(AudioFilter(), true);
    ASSERT_TRUE(
        original.SetEventProjection({core::TimelineEventField::kRecordIn})
            .has_value());
    ASSERT_TRUE(original.Create("Persisted").has_value());
    const auto identifier = *original.ActiveIdentifier();

    TimelineTemplateViewModel restored{directory.path()};
    const auto load_result = restored.Load();
    ASSERT_TRUE(load_result.has_value());
    EXPECT_TRUE(load_result->empty());
    ASSERT_TRUE(restored.SelectTemplate(identifier));
    EXPECT_EQ(restored.FilterQuery(), AudioFilter());
    EXPECT_TRUE(
        std::ranges::equal(restored.EventProjection(),
                           std::vector{core::TimelineEventField::kRecordIn}));
}

TEST(TimelineTemplateViewModelTest, RejectsInvalidOrIncompleteCommands) {
    const TemporaryTemplateDirectory directory;
    TimelineTemplateViewModel view_model{directory.path()};
    ASSERT_TRUE(view_model.Load().has_value());

    const auto no_active = view_model.UpdateActive();
    ASSERT_FALSE(no_active.has_value());
    EXPECT_EQ(CommandError(no_active),
              TimelineTemplateCommandError::kNoActiveTemplate);

    view_model.SetFilterState(AudioFilter(), false);
    const auto invalid_filter = view_model.Create("Invalid filter");
    ASSERT_FALSE(invalid_filter.has_value());
    EXPECT_EQ(CommandError(invalid_filter),
              TimelineTemplateCommandError::kInvalidFilter);

    const auto invalid_projection = view_model.SetEventProjection({});
    ASSERT_FALSE(invalid_projection.has_value());
    EXPECT_EQ(CommandError(invalid_projection),
              TimelineTemplateCommandError::kInvalidProjection);
}

} // namespace
} // namespace edit_atlas::presentation
