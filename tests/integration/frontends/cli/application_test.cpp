#include <edit_atlas/frontends/cli/application.hpp>

#include <edit_atlas/test/media_fixture.hpp>

#include <gtest/gtest.h>
#include <minizip/unzip.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <random>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace edit_atlas::frontends::cli {
namespace {

[[nodiscard]] std::string PathToUtf8(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return std::string{reinterpret_cast<const char *>(utf8.data()),
                       utf8.size()};
}

[[nodiscard]] std::filesystem::path PathFromUtf8(std::string_view value) {
    const auto *begin = reinterpret_cast<const char8_t *>(value.data());
    return std::filesystem::path{std::u8string{begin, begin + value.size()}};
}

[[nodiscard]] std::filesystem::path FixturePath(std::string_view name) {
    return std::filesystem::path{EDIT_ATLAS_FRONTENDS_CLI_FIXTURE_DIRECTORY} /
           name;
}

[[nodiscard]] std::filesystem::path
UniqueTemporaryPath(std::string_view name, std::string_view extension) {
    std::random_device random;
    const auto filename =
        std::string{name} + "-" +
        std::to_string(static_cast<unsigned long long>(random())) +
        std::string{extension};
    return std::filesystem::temp_directory_path() / PathFromUtf8(filename);
}

class TemporaryPath final {
  public:
    TemporaryPath(std::string_view name, std::string_view extension)
        : path_(UniqueTemporaryPath(name, extension)) {
        Remove();
    }

    ~TemporaryPath(void) { Remove(); }

    TemporaryPath(const TemporaryPath &) = delete;
    TemporaryPath &operator=(const TemporaryPath &) = delete;
    TemporaryPath(TemporaryPath &&) = delete;
    TemporaryPath &operator=(TemporaryPath &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

  private:
    void Remove(void) const {
        std::error_code error;
        static_cast<void>(std::filesystem::remove(path_, error));
    }

    std::filesystem::path path_;
};

class TemporaryDocument final {
  public:
    TemporaryDocument(std::string_view name, std::string_view content)
        : path_(UniqueTemporaryPath(name, ".edl")) {
        std::ofstream output{path_, std::ios::binary | std::ios::trunc};
        output.write(content.data(),
                     static_cast<std::streamsize>(content.size()));
        valid_ = static_cast<bool>(output);
    }

    ~TemporaryDocument(void) {
        std::error_code error;
        static_cast<void>(std::filesystem::remove(path_, error));
    }

    TemporaryDocument(const TemporaryDocument &) = delete;
    TemporaryDocument &operator=(const TemporaryDocument &) = delete;
    TemporaryDocument(TemporaryDocument &&) = delete;
    TemporaryDocument &operator=(TemporaryDocument &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

    [[nodiscard]] bool valid(void) const noexcept { return valid_; }

  private:
    std::filesystem::path path_;
    bool valid_ = false;
};

struct RunResult final {
    ExitCode exit_code;
    std::string output;
    std::string error;
};

[[nodiscard]] RunResult
RunWithArguments(const std::vector<std::string> &arguments) {
    std::vector<std::string_view> views;
    views.reserve(arguments.size());
    for (const auto &argument : arguments) {
        views.emplace_back(argument);
    }
    std::ostringstream output;
    std::ostringstream error;
    const auto exit_code =
        Run(std::span<const std::string_view>{views}, output, error);
    return RunResult{
        .exit_code = exit_code,
        .output = output.str(),
        .error = error.str(),
    };
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path &path) {
    std::ifstream input{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
}

void WriteFile(const std::filesystem::path &path, std::string_view content) {
    std::ofstream output{
        path,
        std::ios::binary | std::ios::out | std::ios::trunc,
    };
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
}

[[nodiscard]] std::optional<std::string>
ReadZipEntry(const std::filesystem::path &path, std::string_view entry_name) {
    const auto path_text = path.string();
    auto *archive = unzOpen64(path_text.c_str());
    if (archive == nullptr) {
        return std::nullopt;
    }

    const std::string name{entry_name};
    if (unzLocateFile(archive, name.c_str(), 0) != UNZ_OK) {
        unzClose(archive);
        return std::nullopt;
    }

    unz_file_info64 information{};
    if (unzGetCurrentFileInfo64(archive, &information, nullptr, 0, nullptr, 0,
                                nullptr, 0) != UNZ_OK ||
        unzOpenCurrentFile(archive) != UNZ_OK) {
        unzClose(archive);
        return std::nullopt;
    }

    std::string content(static_cast<std::size_t>(information.uncompressed_size),
                        '\0');
    std::size_t offset = 0;
    while (offset < content.size()) {
        const auto remaining = content.size() - offset;
        const auto chunk_size =
            remaining > static_cast<std::size_t>(0x7FFF'FFFF)
                ? 0x7FFF'FFFFU
                : static_cast<unsigned int>(remaining);
        const auto bytes_read =
            unzReadCurrentFile(archive, content.data() + offset, chunk_size);
        if (bytes_read <= 0) {
            unzCloseCurrentFile(archive);
            unzClose(archive);
            return std::nullopt;
        }
        offset += static_cast<std::size_t>(bytes_read);
    }

    const auto entry_close_result = unzCloseCurrentFile(archive);
    const auto archive_close_result = unzClose(archive);
    if (entry_close_result != UNZ_OK || archive_close_result != UNZ_OK) {
        return std::nullopt;
    }
    return content;
}

TEST(CliApplicationTest, ShowsGeneratedHelp) {
    const auto result = RunWithArguments({"--help"});

    EXPECT_EQ(result.exit_code, ExitCode::kSuccess);
    EXPECT_NE(result.output.find("edit-atlas-cli"), std::string::npos);
    EXPECT_NE(result.output.find("convert"), std::string::npos);
    EXPECT_TRUE(result.error.empty());
}

TEST(CliApplicationTest, ShowsApplicationVersion) {
    const auto result = RunWithArguments({"--version"});

    EXPECT_EQ(result.exit_code, ExitCode::kSuccess);
    EXPECT_NE(result.output.find("Edit Atlas "), std::string::npos);
    EXPECT_TRUE(result.error.empty());
}

TEST(CliApplicationTest, RequiresACommand) {
    const auto result = RunWithArguments({});

    EXPECT_EQ(result.exit_code, ExitCode::kUsageError);
    EXPECT_TRUE(result.output.empty());
    EXPECT_NE(result.error.find("subcommand"), std::string::npos);
}

TEST(CliApplicationTest, ConvertsTimelineToXlsx) {
    const TemporaryPath destination{"edit-atlas-cli-convert", ".xlsx"};
    const auto result = RunWithArguments(
        {"convert", "--fps", "24", PathToUtf8(FixturePath("mixed_tracks.edl")),
         PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kSuccess);
    EXPECT_NE(result.output.find("Converted 4 event(s)"), std::string::npos);
    EXPECT_TRUE(result.error.empty());
    const auto content = ReadFile(destination.path());
    ASSERT_GE(content.size(), 2);
    EXPECT_EQ(content[0], 'P');
    EXPECT_EQ(content[1], 'K');
}

TEST(CliApplicationTest, AcceptsEveryExistingWorkbookOption) {
    const TemporaryPath destination{"edit-atlas-cli-options", ".xlsx"};
    const auto result = RunWithArguments(
        {"convert", "--fps=24", "--language", "en", "--no-timeline-sheet",
         "--no-diagnostics-sheet", PathToUtf8(FixturePath("mixed_tracks.edl")),
         PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kSuccess);
    const auto workbook = ReadZipEntry(destination.path(), "xl/workbook.xml");
    const auto shared_strings =
        ReadZipEntry(destination.path(), "xl/sharedStrings.xml");
    ASSERT_TRUE(workbook.has_value());
    ASSERT_TRUE(shared_strings.has_value());
    EXPECT_NE(workbook->find("name=\"Events\""), std::string::npos);
    EXPECT_EQ(workbook->find("name=\"Timeline\""), std::string::npos);
    EXPECT_EQ(workbook->find("name=\"Diagnostics\""), std::string::npos);
    EXPECT_NE(shared_strings->find("Edit Type"), std::string::npos);
}

TEST(CliApplicationTest, SelectsEventColumns) {
    const TemporaryPath destination{"edit-atlas-cli-columns", ".xlsx"};
    const auto result = RunWithArguments(
        {"convert", "--fps=24", "--language=en", "--columns=comments,event",
         PathToUtf8(FixturePath("mixed_tracks.edl")),
         PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kSuccess);
    const auto shared_strings =
        ReadZipEntry(destination.path(), "xl/sharedStrings.xml");
    ASSERT_TRUE(shared_strings.has_value());
    EXPECT_NE(shared_strings->find("<t>Comments</t>"), std::string::npos);
    EXPECT_NE(shared_strings->find("<t>Event</t>"), std::string::npos);
    EXPECT_EQ(shared_strings->find("<t>Reel</t>"), std::string::npos);
}

TEST(CliApplicationTest, EmbedsInitialFramesFromMatchingRenderedVideo) {
    const TemporaryPath video{"edit-atlas-cli-video", ".mov"};
    const TemporaryPath destination{"edit-atlas-cli-frames", ".xlsx"};
    auto video_options = media::test::VideoFixtureOptions{};
    video_options.frame_rate_numerator = 24;
    video_options.frame_count = 96;
    video_options.starting_timecode = "01:00:00:00";
    const auto fixture =
        media::test::WriteVideoFixture(video.path(), "mov", video_options);
    ASSERT_TRUE(fixture.has_value())
        << (fixture.has_value() ? "" : fixture.error());

    const auto result = RunWithArguments(
        {"convert", "--fps=24", "--language=en",
         "--columns=initial-frame,event", "--video", PathToUtf8(video.path()),
         PathToUtf8(FixturePath("mixed_tracks.edl")),
         PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kSuccess);
    EXPECT_NE(result.output.find("Embedded initial frames"), std::string::npos);
    EXPECT_TRUE(
        ReadZipEntry(destination.path(), "xl/media/image1.png").has_value());
    EXPECT_TRUE(
        ReadZipEntry(destination.path(), "xl/media/image4.png").has_value());
}

TEST(CliApplicationTest, RequiresVideoForInitialFrameColumn) {
    const auto result =
        RunWithArguments({"convert", "--columns=initial-frame,event",
                          "input.edl", "output.xlsx"});

    EXPECT_EQ(result.exit_code, ExitCode::kUsageError);
    EXPECT_NE(result.error.find("--video is required"), std::string::npos);
}

TEST(CliApplicationTest, RejectsUnusedVideoInput) {
    const auto result = RunWithArguments(
        {"convert", "--video=render.mov", "input.edl", "output.xlsx"});

    EXPECT_EQ(result.exit_code, ExitCode::kUsageError);
    EXPECT_NE(result.error.find("--video requires initial-frame"),
              std::string::npos);
}

TEST(CliApplicationTest, RejectsDuplicateEventColumns) {
    const auto result = RunWithArguments(
        {"convert", "--columns=event,event", "input.edl", "output.xlsx"});

    EXPECT_EQ(result.exit_code, ExitCode::kUsageError);
    EXPECT_TRUE(result.output.empty());
    EXPECT_NE(result.error.find("unique column identifiers"),
              std::string::npos);
}

TEST(CliApplicationTest, PreservesExistingDestinationWithoutForce) {
    const TemporaryPath destination{"edit-atlas-cli-existing", ".xlsx"};
    WriteFile(destination.path(), "original");

    const auto result = RunWithArguments(
        {"convert", "--fps", "24", PathToUtf8(FixturePath("mixed_tracks.edl")),
         PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kDestinationExists);
    EXPECT_EQ(ReadFile(destination.path()), "original");
    EXPECT_NE(result.error.find("cli.output.destination_exists"),
              std::string::npos);
}

TEST(CliApplicationTest, ReplacesExistingDestinationWithForce) {
    const TemporaryPath destination{"edit-atlas-cli-force", ".xlsx"};
    WriteFile(destination.path(), "original");

    const auto result =
        RunWithArguments({"convert", "--force", "--fps", "24",
                          PathToUtf8(FixturePath("mixed_tracks.edl")),
                          PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kSuccess);
    const auto content = ReadFile(destination.path());
    ASSERT_GE(content.size(), 2);
    EXPECT_EQ(content[0], 'P');
    EXPECT_EQ(content[1], 'K');
}

TEST(CliApplicationTest, ReturnsWarningsAfterCreatingTheWorkbook) {
    const TemporaryDocument document{
        "edit-atlas-cli-warning",
        "TITLE: WARNING\n"
        "FCM: NON-DROP FRAME\n"
        "UNRECOGNIZED CONTENT\n",
    };
    const TemporaryPath destination{"edit-atlas-cli-warning", ".xlsx"};
    ASSERT_TRUE(document.valid());

    const auto result =
        RunWithArguments({"convert", "--fps", "24", PathToUtf8(document.path()),
                          PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kWarnings);
    EXPECT_TRUE(std::filesystem::exists(destination.path()));
    EXPECT_NE(result.error.find("warning [cmx3600.unknown_content]"),
              std::string::npos);
    EXPECT_NE(result.error.find(PathToUtf8(document.path()) + ":3:1:"),
              std::string::npos);
}

TEST(CliApplicationTest, RejectsInvalidInputWithoutCreatingTheWorkbook) {
    const TemporaryPath destination{"edit-atlas-cli-invalid", ".xlsx"};
    const auto result = RunWithArguments(
        {"convert", PathToUtf8(FixturePath("mixed_tracks.edl")),
         PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kInvalidInput);
    EXPECT_FALSE(std::filesystem::exists(destination.path()));
    EXPECT_NE(result.error.find("cmx3600.missing_frame_rate"),
              std::string::npos);
}

TEST(CliApplicationTest, ReturnsOperationalFailureForMissingInput) {
    const auto missing = UniqueTemporaryPath("edit-atlas-cli-missing", ".edl");
    const TemporaryPath destination{"edit-atlas-cli-missing", ".xlsx"};
    std::error_code removal_error;
    static_cast<void>(std::filesystem::remove(missing, removal_error));

    const auto result = RunWithArguments(
        {"convert", PathToUtf8(missing), PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kOperationalFailure);
    EXPECT_FALSE(std::filesystem::exists(destination.path()));
    EXPECT_NE(result.error.find("cli.input.open_failed"), std::string::npos);
}

TEST(CliApplicationTest, PreservesUtf8InputAndOutputPaths) {
    const TemporaryDocument document{
        "edit-atlas-cli-edição",
        "TITLE: EDIÇÃO\n"
        "FCM: NON-DROP FRAME\n",
    };
    const TemporaryPath destination{"edit-atlas-cli-relatório", ".xlsx"};
    ASSERT_TRUE(document.valid());

    const auto result =
        RunWithArguments({"convert", "--fps", "24", PathToUtf8(document.path()),
                          PathToUtf8(destination.path())});

    EXPECT_EQ(result.exit_code, ExitCode::kSuccess);
    EXPECT_TRUE(std::filesystem::exists(destination.path()));
    EXPECT_NE(result.output.find(PathToUtf8(document.path())),
              std::string::npos);
    EXPECT_NE(result.output.find(PathToUtf8(destination.path())),
              std::string::npos);
}

TEST(CliApplicationTest, RejectsUnsupportedWorkbookLanguage) {
    const auto result = RunWithArguments(
        {"convert", "--language", "es", "input.edl", "output.xlsx"});

    EXPECT_EQ(result.exit_code, ExitCode::kUsageError);
    EXPECT_TRUE(result.output.empty());
    EXPECT_NE(result.error.find("--language"), std::string::npos);
    EXPECT_NE(result.error.find("en"), std::string::npos);
    EXPECT_NE(result.error.find("pt-BR"), std::string::npos);
}

} // namespace
} // namespace edit_atlas::frontends::cli
