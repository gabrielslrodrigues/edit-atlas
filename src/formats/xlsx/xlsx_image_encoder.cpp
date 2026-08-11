#include <edit_atlas/formats/xlsx/detail/xlsx_image_encoder.hpp>

#include <png.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace edit_atlas::formats::xlsx::detail {
namespace {

[[nodiscard]] bool ValidImage(const core::RgbImage &image,
                              std::int32_t maximum_width,
                              std::int32_t maximum_height) {
    if (image.width <= 0 || image.height <= 0 || maximum_width <= 0 ||
        maximum_height <= 0 || image.width > maximum_width ||
        image.height > maximum_height ||
        image.width > std::numeric_limits<std::int32_t>::max() / 3) {
        return false;
    }
    const auto minimum_stride = static_cast<std::size_t>(image.width) * 3U;
    return image.row_stride >= minimum_stride &&
           image.row_stride <= static_cast<std::size_t>(
                                   std::numeric_limits<png_int_32>::max()) &&
           static_cast<std::size_t>(image.height) <=
               std::numeric_limits<std::size_t>::max() / image.row_stride &&
           image.pixels.size() >=
               image.row_stride * static_cast<std::size_t>(image.height);
}

} // namespace

std::expected<std::vector<std::byte>, std::string>
EncodeRgbImageAsPng(const core::RgbImage &image, std::int32_t maximum_width,
                    std::int32_t maximum_height) {
    if (!ValidImage(image, maximum_width, maximum_height)) {
        return std::unexpected(
            "The RGB image is invalid or exceeds the requested size limit.");
    }

    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    png.width = static_cast<png_uint_32>(image.width);
    png.height = static_cast<png_uint_32>(image.height);
    png.format = PNG_FORMAT_RGB;
    png_alloc_size_t output_size = 0;
    const auto row_stride = static_cast<png_int_32>(image.row_stride);
    if (png_image_write_to_memory(&png, nullptr, &output_size, 0,
                                  image.pixels.data(), row_stride,
                                  nullptr) == 0) {
        auto message =
            std::string{"Could not determine the encoded PNG size: "} +
            png.message;
        png_image_free(&png);
        return std::unexpected(std::move(message));
    }

    std::vector<std::byte> output(static_cast<std::size_t>(output_size));
    if (png_image_write_to_memory(&png, output.data(), &output_size, 0,
                                  image.pixels.data(), row_stride,
                                  nullptr) == 0) {
        auto message = std::string{"Could not encode the RGB image as PNG: "} +
                       png.message;
        png_image_free(&png);
        return std::unexpected(std::move(message));
    }
    png_image_free(&png);
    output.resize(static_cast<std::size_t>(output_size));
    return output;
}

} // namespace edit_atlas::formats::xlsx::detail
