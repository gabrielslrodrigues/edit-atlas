#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/document_service.hpp>

#include <edit_atlas/core/editorial_timeline.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace edit_atlas::services {
namespace {

[[nodiscard]] std::filesystem::path
UniqueTemporaryPath(std::string_view name) {
    std::random_device random;
    return std::filesystem::temp_directory_path() /
           (std::string{name} + "-" +
            std::to_string(static_cast<unsigned long long>(random())) +
            ".edl");
}

class TemporaryDocument final {
  public:
    TemporaryDocument(std::string_view name, std::string_view content)
        : path_(UniqueTemporaryPath(name)) {
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

TEST(DocumentServiceTest, OpensDocumentWithStandardCppRequest) {
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const TemporaryDocument file{
        "edit-atlas-services-valid",
        "TITLE: SERVICES TEST\n"
        "FCM: NON-DROP FRAME\n"
        "001 AX V C 00:00:00:00 00:00:01:00 "
        "01:00:00:00 01:00:01:00\n",
    };
    ASSERT_TRUE(file.valid());
    const DocumentService service{*registry};

    const auto result = service.OpenDocument(OpenDocumentRequest{
        .path = file.path(),
        .format_identifier = {},
        .options =
            {
                core::MetadataEntry{
                    .key = std::string{formats::cmx3600::kFrameRateOption},
                    .value = "24",
                },
            },
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->path, file.path());
    EXPECT_EQ(result->document.title, "SERVICES TEST");
    ASSERT_EQ(result->document.events.size(), 1);
    EXPECT_EQ(result->document.events.front().identifier, "001");
}

TEST(DocumentServiceTest, ReturnsImportFailureWithDiagnostics) {
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const TemporaryDocument file{
        "edit-atlas-services-missing-frame-rate",
        "TITLE: SERVICES TEST\n"
        "FCM: NON-DROP FRAME\n"
        "001 AX V C 00:00:00:00 00:00:01:00 "
        "01:00:00:00 01:00:01:00\n",
    };
    ASSERT_TRUE(file.valid());
    const DocumentService service{*registry};

    const auto result = service.OpenDocument(OpenDocumentRequest{
        .path = file.path(),
        .format_identifier = {},
        .options = {},
    });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, DocumentOpenFailureKind::kImportFailed);
    EXPECT_TRUE(std::ranges::any_of(
        result.error().diagnostics, [](const core::Diagnostic &diagnostic) {
            return diagnostic.code ==
                   formats::cmx3600::diagnostic_code::kMissingFrameRate;
        }));
}

TEST(DocumentServiceTest, ReturnsOpenFailureForMissingFile) {
    auto registry = CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    const DocumentService service{*registry};
    const auto path =
        UniqueTemporaryPath("edit-atlas-services-does-not-exist");
    std::error_code error;
    static_cast<void>(std::filesystem::remove(path, error));

    const auto result = service.OpenDocument(OpenDocumentRequest{
        .path = path,
        .format_identifier = {},
        .options = {},
    });

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().path, path);
    EXPECT_EQ(result.error().kind, DocumentOpenFailureKind::kOpenFailed);
    EXPECT_TRUE(result.error().filesystem_error);
    EXPECT_FALSE(result.error().filesystem_error.message().empty());
    EXPECT_TRUE(result.error().diagnostics.empty());
}

} // namespace
} // namespace edit_atlas::services
