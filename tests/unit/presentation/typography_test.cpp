#include <edit_atlas/presentation/typography.hpp>

#include <QString>

#include <gtest/gtest.h>

namespace edit_atlas::presentation {
namespace {

TEST(TypographyTest, DescribesAnAscendingSizeHierarchy) {
    const auto &typography = ApplicationTypographyPolicy();
    EXPECT_LT(typography.bodyPointSize, typography.headingPointSize);
    EXPECT_LT(typography.headingPointSize, typography.titlePointSize);
}

TEST(TypographyTest, UsesIncreasingWeightsWithinTheSupportedFaces) {
    const auto &typography = ApplicationTypographyPolicy();
    EXPECT_EQ(typography.bodyWeight, 400);
    EXPECT_EQ(typography.headingWeight, 500);
    EXPECT_EQ(typography.titleWeight, 600);
    EXPECT_FALSE(typography.family.isEmpty());
}

TEST(TypographyTest, BundlesOneResourcePerWeight) {
    auto paths = BundledTypographyResourcePaths();
    ASSERT_EQ(paths.size(), 3);
    for (const auto &path : paths) {
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

} // namespace
} // namespace edit_atlas::presentation
