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

#include "edit_atlas/presentation/typography.hpp"

#include "QString"
#include "gtest/gtest.h"

namespace edit_atlas::presentation {
namespace {

TEST(TypographyTest, DescribesAnAscendingSizeHierarchy) {
  const auto& typography = ApplicationTypographyPolicy();
  EXPECT_LT(typography.body_point_size, typography.heading_point_size);
  EXPECT_LT(typography.heading_point_size, typography.title_point_size);
}

TEST(TypographyTest, UsesIncreasingWeightsWithinTheSupportedFaces) {
  const auto& typography = ApplicationTypographyPolicy();
  EXPECT_EQ(typography.body_weight, 400);
  EXPECT_EQ(typography.heading_weight, 500);
  EXPECT_EQ(typography.title_weight, 600);
  EXPECT_FALSE(typography.family.isEmpty());
}

TEST(TypographyTest, BundlesOneResourcePerWeight) {
  auto paths = BundledTypographyResourcePaths();
  ASSERT_EQ(paths.size(), 3);
  for (const auto& path : paths) {
    EXPECT_TRUE(path.startsWith(QStringLiteral(":/fonts/")));
    EXPECT_TRUE(path.endsWith(QStringLiteral(".ttf")));
  }
  EXPECT_EQ(paths.removeDuplicates(), 0);
}

TEST(TypographyTest, KeepsThePlatformFamilyWhenRegistrationCannotSucceed) {
  // Unit tests run without a GUI application and without the font
  // resource, which is the same situation as a failed registration: the
  // frontend must be told to keep the platform family rather than to name
  // one that cannot resolve.
  EXPECT_FALSE(RegisterBundledTypography());
  EXPECT_TRUE(ResolvedTypographyFamily().isEmpty());
}

}  // namespace
}  // namespace edit_atlas::presentation
