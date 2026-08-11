#ifndef EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_IMAGE_ENCODER_HPP_
#define EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_IMAGE_ENCODER_HPP_

#include <edit_atlas/core/rgb_image.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace edit_atlas::formats::xlsx::detail {

/// Encodes a valid size-limited RGB24 image as PNG bytes.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
EncodeRgbImageAsPng(const core::RgbImage &image, std::int32_t maximum_width,
                    std::int32_t maximum_height);

} // namespace edit_atlas::formats::xlsx::detail

#endif // EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_IMAGE_ENCODER_HPP_
