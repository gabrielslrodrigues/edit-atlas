#ifndef EDIT_ATLAS_SERVICES_BUILT_IN_FORMATS_HPP_
#define EDIT_ATLAS_SERVICES_BUILT_IN_FORMATS_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <expected>

namespace edit_atlas::services {

/// Creates a registry containing every format built into Edit Atlas.
[[nodiscard]] std::expected<core::FormatRegistry, core::FormatRegistrationError>
CreateBuiltInFormatRegistry(void);

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_BUILT_IN_FORMATS_HPP_
