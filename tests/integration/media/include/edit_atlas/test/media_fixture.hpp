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

#ifndef EDIT_ATLAS_TEST_MEDIA_FIXTURE_HPP_
#define EDIT_ATLAS_TEST_MEDIA_FIXTURE_HPP_

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace edit_atlas::media::test {

enum class FixtureVideoCodec {
  kMpeg2Video,
  kRawVideo,
};

struct VideoFixtureOptions final {
  FixtureVideoCodec codec = FixtureVideoCodec::kMpeg2Video;
  std::int32_t frame_rate_numerator = 25;
  std::int32_t frame_rate_denominator = 1;
  std::int32_t frame_count = 3;
  std::optional<std::string> starting_timecode;
};

[[nodiscard]] std::expected<void, std::string> WriteVideoFixture(
    const std::filesystem::path& path, std::string_view container_name,
    const VideoFixtureOptions& options);

}  // namespace edit_atlas::media::test

#endif  // EDIT_ATLAS_TEST_MEDIA_FIXTURE_HPP_
