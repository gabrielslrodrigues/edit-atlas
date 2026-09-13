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

#include "edit_atlas/services/timeline_template.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

namespace edit_atlas::services {
namespace {

void AppendHex(std::string& output, std::uint32_t value) {
  std::array<char, 8> buffer{};
  const auto result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
  const auto digits = static_cast<std::size_t>(result.ptr - buffer.data());
  output.append(buffer.size() - digits, '0');
  output.append(buffer.data(), result.ptr);
}

}  // namespace

std::string GenerateTimelineTemplateIdentifier(void) {
  std::random_device random;
  std::string identifier;
  identifier.reserve(32);
  for (int part = 0; part < 4; ++part) {
    AppendHex(identifier, static_cast<std::uint32_t>(random()));
  }
  return identifier;
}

}  // namespace edit_atlas::services
