#include <edit_atlas/core/version.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace edit_atlas::core {
namespace {

TEST(VersionTest, ReturnsProjectVersion) {
    EXPECT_EQ(Version(), std::string_view{"0.1.1"});
}

} // namespace
} // namespace edit_atlas::core
