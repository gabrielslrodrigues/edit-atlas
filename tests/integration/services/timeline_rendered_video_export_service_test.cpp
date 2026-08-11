#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/timeline_frame_extraction_service.hpp>
#include <edit_atlas/services/timeline_rendered_video_export_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/test/media_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace edit_atlas::services {
namespace {

[[nodiscard]] std::filesystem::path TemporaryPath(std::string_view extension) {
    std::random_device random;
    return std::filesystem::temp_directory_path() /
           ("edit-atlas-rendered-export-" +
            std::to_string(static_cast<unsigned long long>(random())) +
            std::string{extension});
}

class TemporaryFiles final {
  public:
    TemporaryFiles(void)
        : video_{TemporaryPath(".mov")}, workbook_{TemporaryPath(".xlsx")} {}

    ~TemporaryFiles(void) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove(video_, error));
        error.clear();
        static_cast<void>(std::filesystem::remove(workbook_, error));
    }

    TemporaryFiles(const TemporaryFiles &) = delete;
    TemporaryFiles &operator=(const TemporaryFiles &) = delete;
    TemporaryFiles(TemporaryFiles &&) = delete;
    TemporaryFiles &operator=(TemporaryFiles &&) = delete;

    [[nodiscard]] const std::filesystem::path &Video(void) const noexcept {
        return video_;
    }
    [[nodiscard]] const std::filesystem::path &Workbook(void) const noexcept {
        return workbook_;
    }

  private:
    std::filesystem::path video_;
    std::filesystem::path workbook_;
};

[[nodiscard]] core::EditEvent Event(std::string identifier,
                                    std::int64_t record_frame,
                                    const core::FrameRate &rate) {
    auto source_start = core::Timecode::FromFrameCount(
                            0, rate, core::TimecodeMode::kNonDropFrame)
                            .value();
    auto source_end = core::Timecode::FromFrameCount(
                          1, rate, core::TimecodeMode::kNonDropFrame)
                          .value();
    auto record_start =
        core::Timecode::FromFrameCount(record_frame, rate,
                                       core::TimecodeMode::kNonDropFrame)
            .value();
    auto record_end =
        core::Timecode::FromFrameCount(record_frame + 1, rate,
                                       core::TimecodeMode::kNonDropFrame)
            .value();
    return core::EditEvent{
        .identifier = std::move(identifier),
        .reel = "AX",
        .track =
            core::Track{.kind = core::TrackKind::kVideo, .identifier = "V"},
        .edit_type = core::EditType::kCut,
        .transition = std::nullopt,
        .source_range = core::TimecodeRange::Create(std::move(source_start),
                                                    std::move(source_end))
                            .value(),
        .record_range = core::TimecodeRange::Create(std::move(record_start),
                                                    std::move(record_end))
                            .value(),
        .comments = {},
        .metadata = {},
        .provenance = std::nullopt,
    };
}

[[nodiscard]] core::TimelineDocument Timeline(void) {
    const auto rate = core::FrameRate::Create(25, 1).value();
    return core::TimelineDocument{
        .title = "Rendered export fixture",
        .frame_rate = rate,
        .timecode_mode = core::TimecodeMode::kNonDropFrame,
        .events = {Event("001", 90'000, rate), Event("002", 90'002, rate)},
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
}

TEST(TimelineRenderedVideoExportServiceTest,
     RequiresVideoForInitialFrameProjection) {
    TemporaryFiles files;
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    auto reference_timeline = Timeline();
    const TimelineRenderedVideoExportService service{*registry};

    const auto result = service.Export(TimelineRenderedVideoExportRequest{
        .document_export =
            TimelineDocumentExportRequest{
                .path = files.Workbook(),
                .format_identifier =
                    std::string{formats::xlsx::kFormatIdentifier},
                .timeline = reference_timeline,
                .event_projection =
                    {
                        core::TimelineEventField::kInitialFrame,
                    },
                .options = {},
                .event_images = {},
                .replace_existing = false,
            },
        .reference_timeline = std::move(reference_timeline),
        .video_path = std::nullopt,
    });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind,
              TimelineRenderedVideoExportFailureKind::kVideoRequired);
    ASSERT_EQ(result.error().diagnostics.size(), 1);
    EXPECT_EQ(result.error().diagnostics.front().code,
              timeline_rendered_video_export_diagnostic_code::kVideoRequired);
    EXPECT_FALSE(std::filesystem::exists(files.Workbook()));
}

TEST(TimelineRenderedVideoExportServiceTest,
     MapsFilteredEventsToTheirExportDocumentRows) {
    TemporaryFiles files;
    auto fixture_options = media::test::VideoFixtureOptions{};
    fixture_options.starting_timecode = "01:00:00:00";
    const auto fixture =
        media::test::WriteVideoFixture(files.Video(), "mov", fixture_options);
    ASSERT_TRUE(fixture.has_value())
        << (fixture.has_value() ? "" : fixture.error());
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    auto reference_timeline = Timeline();
    auto export_timeline = reference_timeline;
    export_timeline.events = {reference_timeline.events[1]};
    std::vector<TimelineFrameExtractionProgress> progress;
    const TimelineRenderedVideoExportService service{*registry};

    const auto result = service.Export(
        TimelineRenderedVideoExportRequest{
            .document_export =
                TimelineDocumentExportRequest{
                    .path = files.Workbook(),
                    .format_identifier =
                        std::string{formats::xlsx::kFormatIdentifier},
                    .timeline = std::move(export_timeline),
                    .event_projection =
                        {
                            core::TimelineEventField::kInitialFrame,
                            core::TimelineEventField::kEventIdentifier,
                        },
                    .options = {},
                    .event_images = {},
                    .replace_existing = false,
                },
            .reference_timeline = std::move(reference_timeline),
            .video_path = files.Video(),
        },
        {}, [&progress](const TimelineFrameExtractionProgress &update) {
            progress.push_back(update);
        });

    ASSERT_TRUE(result.has_value())
        << (result.has_value() ? ""
                               : result.error().diagnostics.front().message);
    EXPECT_EQ(result->unique_frame_count, 1);
    EXPECT_TRUE(result->video_information.has_value());
    EXPECT_TRUE(result->video_mapping.has_value());
    EXPECT_TRUE(std::filesystem::exists(files.Workbook()));
    ASSERT_FALSE(progress.empty());
    EXPECT_EQ(progress.back().completed_events, 1);
    EXPECT_EQ(progress.back().total_events, 1);
}

} // namespace
} // namespace edit_atlas::services
