#include <edit_atlas/services/built_in_formats.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>
#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <memory>

namespace edit_atlas::services {

std::expected<core::FormatRegistry, core::FormatRegistrationError>
CreateBuiltInFormatRegistry(void) {
    core::FormatRegistry registry;
    if (const auto result = registry.RegisterImporter(
            std::make_unique<formats::cmx3600::Cmx3600Importer>());
        !result.has_value()) {
        return std::unexpected(result.error());
    }
    if (const auto result = registry.RegisterExporter(
            std::make_unique<formats::xlsx::XlsxExporter>());
        !result.has_value()) {
        return std::unexpected(result.error());
    }
    return registry;
}

} // namespace edit_atlas::services
