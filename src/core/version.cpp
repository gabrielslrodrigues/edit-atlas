#include <edit_atlas/core/version.hpp>

namespace edit_atlas::core {

std::string_view Version(void) noexcept {
    return EDIT_ATLAS_VERSION;
}

} // namespace edit_atlas::core
