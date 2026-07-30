#include <edit_atlas/app/main_window.hpp>
#include <edit_atlas/app/timeline_event_model.hpp>
#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>

#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

#include <optional>

namespace edit_atlas::app {
namespace {

TEST(ApplicationTest, LoadsBrazilianPortugueseTranslations) {
    QTranslator translator;

    ASSERT_TRUE(SetApplicationLanguage(
        translator, ApplicationLanguage::kBrazilianPortuguese));
    EXPECT_EQ(MainWindow::tr("&File"), QStringLiteral("&Arquivo"));
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
