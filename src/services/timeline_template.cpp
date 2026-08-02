#include <edit_atlas/services/timeline_template.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

namespace edit_atlas::services {
namespace {

void AppendHex(std::string &output, std::uint32_t value) {
    std::array<char, 8> buffer{};
    const auto result =
        std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
    const auto digits = static_cast<std::size_t>(result.ptr - buffer.data());
    output.append(buffer.size() - digits, '0');
    output.append(buffer.data(), result.ptr);
}

} // namespace

std::string GenerateTimelineTemplateIdentifier(void) {
    std::random_device random;
    std::string identifier;
    identifier.reserve(32);
    for (int part = 0; part < 4; ++part) {
        AppendHex(identifier, static_cast<std::uint32_t>(random()));
    }
    return identifier;
}

} // namespace edit_atlas::services
