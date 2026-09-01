#include <edit_atlas/presentation/typography.hpp>

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QString>

#include <gtest/gtest.h>

namespace edit_atlas::frontends::widgets {
namespace {

TEST(TypographyIntegrationTest, RegistersTheBundledFamily) {
    ASSERT_TRUE(presentation::RegisterBundledTypography());
    const auto &policy = presentation::ApplicationTypographyPolicy();
    EXPECT_EQ(presentation::ResolvedTypographyFamily(), policy.family);
    EXPECT_TRUE(QFontDatabase::families().contains(policy.family));
}

TEST(TypographyIntegrationTest, AppliesTheSharedPolicyToTheApplication) {
    ASSERT_TRUE(presentation::RegisterBundledTypography());
    presentation::ApplyApplicationTypography();

    const auto &policy = presentation::ApplicationTypographyPolicy();
    const auto font = QApplication::font();
    EXPECT_EQ(font.family(), policy.family);
    EXPECT_DOUBLE_EQ(font.pointSizeF(), policy.bodyPointSize);
    EXPECT_EQ(static_cast<int>(font.weight()), policy.bodyWeight);
}

TEST(TypographyIntegrationTest, ResolvesEveryWeightTheHierarchyUses) {
    ASSERT_TRUE(presentation::RegisterBundledTypography());
    const auto &policy = presentation::ApplicationTypographyPolicy();

    // A missing face resolves to a substitute family rather than failing, so
    // the family reported back is what proves each weight is bundled.
    for (const auto weight :
         {policy.bodyWeight, policy.headingWeight, policy.titleWeight}) {
        QFont font{policy.family};
        font.setWeight(static_cast<QFont::Weight>(weight));
        const QFontInfo info{font};
        EXPECT_EQ(info.family(), policy.family) << "weight " << weight;
    }
}

TEST(TypographyIntegrationTest, SupportsInterfaceCharactersInBothLanguages) {
    ASSERT_TRUE(presentation::RegisterBundledTypography());
    const auto &policy = presentation::ApplicationTypographyPolicy();
    const QFont font{policy.family};

    // Brazilian Portuguese needs these beyond ASCII; a missing glyph would
    // render as a fallback box in the interface.
    for (const auto character : QStringLiteral("áàâãéêíóôõúüçÁÂÃÉÍÓÕÚÇ")) {
        EXPECT_TRUE(QFontMetrics{font}.inFont(character))
            << "missing glyph for code point "
            << static_cast<unsigned int>(character.unicode());
    }
}

} // namespace
} // namespace edit_atlas::frontends::widgets
