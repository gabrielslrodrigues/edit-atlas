#ifndef EDIT_ATLAS_CORE_VERSION_HPP_
#define EDIT_ATLAS_CORE_VERSION_HPP_

#include <string_view>

namespace edit_atlas::core {

[[nodiscard]] std::string_view Version(void) noexcept;

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_VERSION_HPP_
