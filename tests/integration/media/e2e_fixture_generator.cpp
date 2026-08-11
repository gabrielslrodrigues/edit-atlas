#include <edit_atlas/test/media_fixture.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace edit_atlas::media::test {
namespace {

[[nodiscard]] bool WriteFixture(const std::filesystem::path &directory,
                                std::string_view filename,
                                std::int32_t frame_count,
                                std::string_view starting_timecode) {
    const auto options = VideoFixtureOptions{
        .codec = FixtureVideoCodec::kMpeg2Video,
        .frame_rate_numerator = 24,
        .frame_rate_denominator = 1,
        .frame_count = frame_count,
        .starting_timecode = starting_timecode.empty()
                                 ? std::optional<std::string>{}
                                 : std::optional<std::string>{
                                       starting_timecode},
    };
    const auto result =
        WriteVideoFixture(directory / filename, "mov", options);
    if (!result.has_value()) {
        std::cerr << "Could not generate " << filename << ": "
                  << result.error() << '\n';
        return false;
    }
    return true;
}

[[nodiscard]] std::string Timecode(std::int32_t frame) {
    constexpr auto kFramesPerSecond = 24;
    constexpr auto kSecondsPerMinute = 60;
    constexpr auto kMinutesPerHour = 60;
    const auto frames = frame % kFramesPerSecond;
    auto total_seconds = frame / kFramesPerSecond;
    const auto seconds = total_seconds % kSecondsPerMinute;
    total_seconds /= kSecondsPerMinute;
    const auto minutes = total_seconds % kMinutesPerHour;
    const auto hours = total_seconds / kMinutesPerHour;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2)
           << minutes << ':' << std::setw(2) << seconds << ':' << std::setw(2)
           << frames;
    return output.str();
}

[[nodiscard]] bool WriteCancellationEdl(
    const std::filesystem::path &directory, std::int32_t frame_count) {
    std::ofstream output{directory / "cancellation.edl"};
    if (!output) {
        std::cerr << "Could not create cancellation.edl\n";
        return false;
    }
    output << "TITLE: SYNTHETIC CANCELLATION TIMELINE\n"
              "FCM: NON-DROP FRAME\n\n";
    constexpr auto kOneHourAt24Fps = 86'400;
    for (std::int32_t frame = 0; frame < frame_count; ++frame) {
        output << std::setfill('0') << std::setw(3) << frame + 1
               << "  AX       V     C        " << Timecode(frame) << ' '
               << Timecode(frame + 1) << ' '
               << Timecode(kOneHourAt24Fps + frame) << ' '
               << Timecode(kOneHourAt24Fps + frame + 1) << '\n';
    }
    return output.good();
}

} // namespace
} // namespace edit_atlas::media::test

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: edit_atlas_e2e_media_fixture_generator "
                     "<output-directory>\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path output_directory{argv[1]};
    std::error_code error;
    std::filesystem::create_directories(output_directory, error);
    if (error) {
        std::cerr << "Could not create fixture directory: " << error.message()
                  << '\n';
        return EXIT_FAILURE;
    }

    using edit_atlas::media::test::WriteFixture;
    using edit_atlas::media::test::WriteCancellationEdl;
    if (!WriteFixture(output_directory, "matching-render.mov",
                      96, "01:00:00:00") ||
        !WriteFixture(output_directory, "missing-timecode.mov", 96,
                      std::string_view{}) ||
        !WriteFixture(output_directory, "incompatible-timecode.mov",
                      96, "00:00:00:00") ||
        !WriteFixture(output_directory, "cancellation-render.mov", 240,
                      "01:00:00:00") ||
        !WriteCancellationEdl(output_directory, 240)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
