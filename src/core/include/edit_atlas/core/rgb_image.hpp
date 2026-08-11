#ifndef EDIT_ATLAS_CORE_RGB_IMAGE_HPP_
#define EDIT_ATLAS_CORE_RGB_IMAGE_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace edit_atlas::core {

/// An owned top-to-bottom packed RGB24 image.
struct RgbImage final {
    /// Image width in pixels.
    std::int32_t width;
    /// Image height in pixels.
    std::int32_t height;
    /// Number of bytes between consecutive packed rows.
    std::size_t row_stride;
    /// Packed RGB24 pixel data.
    std::vector<std::byte> pixels;
};

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_RGB_IMAGE_HPP_
