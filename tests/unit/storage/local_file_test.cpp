#include <edit_atlas/storage/local_file.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace edit_atlas::storage {
namespace {

class TemporaryDirectory final {
  public:
    TemporaryDirectory(void)
        : path_{
              std::filesystem::path{testing::TempDir()} /
              (std::string{"edit-atlas-storage-"} +
               testing::UnitTest::GetInstance()->current_test_info()->name())} {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory(void) {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;
    TemporaryDirectory(TemporaryDirectory &&) = delete;
    TemporaryDirectory &operator=(TemporaryDirectory &&) = delete;

    [[nodiscard]] const std::filesystem::path &path(void) const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] std::span<const std::byte> Bytes(std::string_view text) {
    return std::as_bytes(std::span{text});
}

TEST(LocalFileTest, WritesAndReadsAllBytes) {
    const TemporaryDirectory directory;
    const auto path = directory.path() / "content.bin";

    ASSERT_TRUE(WriteLocalFileAtomically(path, Bytes("one two"),
                                         ExistingFilePolicy::kPreserve)
                    .has_value());
    const auto result = ReadLocalFile(path);

    ASSERT_TRUE(result.has_value());
    const auto expected = Bytes("one two");
    EXPECT_EQ(*result,
              std::vector<std::byte>(expected.begin(), expected.end()));
}

TEST(LocalFileTest, PreservesOrReplacesAnExistingDestination) {
    const TemporaryDirectory directory;
    const auto path = directory.path() / "content.bin";
    ASSERT_TRUE(WriteLocalFileAtomically(path, Bytes("first"),
                                         ExistingFilePolicy::kPreserve)
                    .has_value());

    const auto preserved = WriteLocalFileAtomically(
        path, Bytes("second"), ExistingFilePolicy::kPreserve);
    ASSERT_FALSE(preserved.has_value());
    EXPECT_EQ(preserved.error().kind, LocalFileFailureKind::kDestinationExists);
    ASSERT_TRUE(WriteLocalFileAtomically(path, Bytes("second"),
                                         ExistingFilePolicy::kReplace)
                    .has_value());
    const auto result = ReadLocalFile(path);
    ASSERT_TRUE(result.has_value());
    const auto expected = Bytes("second");
    EXPECT_EQ(*result,
              std::vector<std::byte>(expected.begin(), expected.end()));
}

TEST(LocalFileTest, ReportsAMissingSource) {
    const TemporaryDirectory directory;

    const auto result = ReadLocalFile(directory.path() / "missing.bin");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, LocalFileFailureKind::kOpenFailed);
}

} // namespace
} // namespace edit_atlas::storage
