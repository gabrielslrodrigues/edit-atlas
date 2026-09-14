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

}  // namespace edit_atlas::core

#endif  // EDIT_ATLAS_CORE_RGB_IMAGE_HPP_
