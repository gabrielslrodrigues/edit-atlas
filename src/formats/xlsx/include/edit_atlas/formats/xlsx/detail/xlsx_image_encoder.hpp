// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_IMAGE_ENCODER_HPP_
#define EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_IMAGE_ENCODER_HPP_

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "edit_atlas/core/rgb_image.hpp"

namespace edit_atlas::formats::xlsx::detail {

/// Encodes a valid size-limited RGB24 image as PNG bytes.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string>
EncodeRgbImageAsPng(const core::RgbImage& image, std::int32_t maximum_width,
                    std::int32_t maximum_height);

}  // namespace edit_atlas::formats::xlsx::detail

#endif  // EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_IMAGE_ENCODER_HPP_
