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

#include <cstddef>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "edit_atlas/frontends/cli/application.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

[[nodiscard]] int Run(std::vector<std::string> arguments) {
  std::vector<std::string_view> views;
  views.reserve(arguments.size());
  for (const auto& argument : arguments) {
    views.emplace_back(argument);
  }
  return static_cast<int>(edit_atlas::frontends::cli::Run(
      std::span<const std::string_view>{views}, std::cout, std::cerr));
}

#if defined(_WIN32)
[[nodiscard]] std::string Utf8Argument(const wchar_t* argument) {
  const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argument,
                                        -1, nullptr, 0, nullptr, nullptr);
  if (size <= 1) {
    return {};
  }
  std::string result(static_cast<std::size_t>(size), '\0');
  static_cast<void>(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argument,
                                        -1, result.data(), size, nullptr,
                                        nullptr));
  result.pop_back();
  return result;
}
#endif

}  // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
  static_cast<void>(SetConsoleOutputCP(CP_UTF8));
  std::vector<std::string> arguments;
  arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int index = 1; index < argc; ++index) {
    arguments.emplace_back(Utf8Argument(argv[index]));
  }
  return Run(std::move(arguments));
}
#else
int main(int argc, char** argv) {
  std::vector<std::string> arguments;
  arguments.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
  for (int index = 1; index < argc; ++index) {
    arguments.emplace_back(argv[index]);
  }
  return Run(std::move(arguments));
}
#endif
