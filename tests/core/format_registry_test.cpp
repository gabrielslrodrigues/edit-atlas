#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include "format_test_doubles.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace edit_atlas::core {
namespace {

[[nodiscard]] FormatDescriptor
Descriptor(std::string identifier, std::string display_name = "Test Format",
           std::string extension = "edl") {
    return FormatDescriptor{
        .identifier = std::move(identifier),
        .display_name = std::move(display_name),
        .extensions = {std::move(extension)},
    };
}

TEST(FormatRegistryTest, RegistersAndDiscoversFormats) {
    FormatRegistry registry;

    ASSERT_TRUE(registry.RegisterImporter(
        std::make_unique<test::StubImporter>(Descriptor("cmx-3600"))));
    ASSERT_TRUE(registry.RegisterImporter(std::make_unique<test::StubImporter>(
        Descriptor("another-edl", "Another EDL", "ale"))));

    const auto formats = registry.importer_formats();
    ASSERT_EQ(formats.size(), 2);
    EXPECT_NE(
        std::ranges::find(formats, "cmx-3600", &FormatDescriptor::identifier),
        formats.end());
    EXPECT_NE(std::ranges::find(formats, "another-edl",
                                &FormatDescriptor::identifier),
              formats.end());
    EXPECT_EQ(registry.FindImporter("cmx-3600")->descriptor().identifier,
              "cmx-3600");
    EXPECT_EQ(registry.FindImporter("missing"), nullptr);
}

TEST(FormatRegistryTest, MatchesExtensionsCaseInsensitively) {
    FormatRegistry registry;
    ASSERT_TRUE(registry.RegisterImporter(
        std::make_unique<test::StubImporter>(Descriptor("cmx-3600"))));

    EXPECT_EQ(registry.ImportersForExtension("edl").size(), 1);
    EXPECT_EQ(registry.ImportersForExtension(".EDL").size(), 1);
    EXPECT_TRUE(registry.ImportersForExtension("xml").empty());
}

TEST(FormatRegistryTest, AllowsImporterAndExporterWithTheSameIdentifier) {
    FormatRegistry registry;

    EXPECT_TRUE(registry.RegisterImporter(
        std::make_unique<test::StubImporter>(Descriptor("cmx-3600"))));
    EXPECT_TRUE(registry.RegisterExporter(
        std::make_unique<test::StubExporter>(Descriptor("cmx-3600"))));

    EXPECT_NE(registry.FindImporter("cmx-3600"), nullptr);
    EXPECT_NE(registry.FindExporter("cmx-3600"), nullptr);
    EXPECT_EQ(registry.ExportersForExtension(".EDL").size(), 1);
}

TEST(FormatRegistryTest, RejectsDuplicateHandlerIdentifiers) {
    FormatRegistry registry;
    ASSERT_TRUE(registry.RegisterImporter(
        std::make_unique<test::StubImporter>(Descriptor("cmx-3600"))));
    ASSERT_TRUE(registry.RegisterExporter(
        std::make_unique<test::StubExporter>(Descriptor("cmx-3600"))));

    EXPECT_EQ(registry
                  .RegisterImporter(std::make_unique<test::StubImporter>(
                      Descriptor("cmx-3600")))
                  .error(),
              FormatRegistrationError::kDuplicateImporter);
    EXPECT_EQ(registry
                  .RegisterExporter(std::make_unique<test::StubExporter>(
                      Descriptor("cmx-3600")))
                  .error(),
              FormatRegistrationError::kDuplicateExporter);
}

TEST(FormatRegistryTest, RejectsInvalidDescriptorsAndNullHandlers) {
    FormatRegistry registry;

    EXPECT_EQ(registry.RegisterImporter(nullptr).error(),
              FormatRegistrationError::kNullHandler);
    EXPECT_EQ(registry
                  .RegisterImporter(std::make_unique<test::StubImporter>(
                      Descriptor("CMX 3600")))
                  .error(),
              FormatRegistrationError::kInvalidIdentifier);
    EXPECT_EQ(registry
                  .RegisterImporter(std::make_unique<test::StubImporter>(
                      Descriptor("cmx-3600", "")))
                  .error(),
              FormatRegistrationError::kEmptyDisplayName);
    EXPECT_EQ(registry
                  .RegisterImporter(std::make_unique<test::StubImporter>(
                      Descriptor("cmx-3600", "CMX 3600", ".EDL")))
                  .error(),
              FormatRegistrationError::kInvalidExtension);
}

TEST(FormatRegistryTest, KeepsImporterAndExporterDiscoveryIndependent) {
    FormatRegistry registry;
    ASSERT_TRUE(registry.RegisterImporter(
        std::make_unique<test::StubImporter>(Descriptor("cmx-3600"))));
    ASSERT_TRUE(registry.RegisterExporter(std::make_unique<test::StubExporter>(
        Descriptor("xlsx", "Excel Workbook", "xlsx"))));

    EXPECT_EQ(registry.importer_formats().size(), 1);
    EXPECT_EQ(registry.exporter_formats().size(), 1);
    EXPECT_EQ(registry.exporter_formats().front().identifier, "xlsx");
    EXPECT_TRUE(registry.ExportersForExtension("edl").empty());
    EXPECT_EQ(registry.ExportersForExtension("xlsx").size(), 1);
}

} // namespace
} // namespace edit_atlas::core
