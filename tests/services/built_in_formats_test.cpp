#include <edit_atlas/services/built_in_formats.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <gtest/gtest.h>

namespace edit_atlas::services {
namespace {

TEST(BuiltInFormatsTest, RegistersEveryBuiltInFormatHandler) {
    const auto registry = CreateBuiltInFormatRegistry();

    ASSERT_TRUE(registry.has_value());
    EXPECT_NE(registry->FindImporter(formats::cmx3600::kFormatIdentifier),
              nullptr);
    EXPECT_NE(registry->FindExporter(formats::xlsx::kFormatIdentifier),
              nullptr);
    EXPECT_EQ(registry->importer_formats().size(), 1);
    EXPECT_EQ(registry->exporter_formats().size(), 1);
}

} // namespace
} // namespace edit_atlas::services
