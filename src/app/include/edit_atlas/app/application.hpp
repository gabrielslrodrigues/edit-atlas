#ifndef EDIT_ATLAS_APP_APPLICATION_HPP_
#define EDIT_ATLAS_APP_APPLICATION_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <expected>

namespace edit_atlas::app {

/// Creates the application's registry with every built-in format handler.
[[nodiscard]] std::expected<core::FormatRegistry, core::FormatRegistrationError>
CreateFormatRegistry(void);

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_APPLICATION_HPP_
