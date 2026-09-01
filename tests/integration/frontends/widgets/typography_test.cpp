#include <edit_atlas/presentation/typography.hpp>

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetrics>
#include <QString>
#include <QtGlobal>

#include <gtest/gtest.h>

namespace edit_atlas::frontends::widgets {
namespace {

// This suite runs headless. The offscreen plugin on Linux and the real
// Windows plugin both load application fonts; the minimal plugin used on
// macOS does not, and reports "This plugin does not support application
// fonts". The expectation is therefore stated per platform rather than
// tolerated everywhere, so a regression where fonts stop registering on a
// platform that supports them still fails.
[[nodiscard]] bool PlatformLoadsApplicationFonts(void) {
#if defined(Q_OS_MACOS)
    return false;
#else
    return true;
#endif
}

TEST(TypographyIntegrationTest, RegistersTheBundledFamilyWhereSupported) {
    const auto registered = presentation::RegisterBundledTypography();
    const auto &policy = presentation::ApplicationTypographyPolicy();

    if (!PlatformLoadsApplicationFonts()) {
        EXPECT_FALSE(registered);
        EXPECT_TRUE(presentation::ResolvedTypographyFamily().isEmpty());
        return;
    }

    ASSERT_TRUE(registered);
    EXPECT_EQ(presentation::ResolvedTypographyFamily(), policy.family);
    EXPECT_TRUE(QFontDatabase::families().contains(policy.family));
}

TEST(TypographyIntegrationTest, AppliesTheSharedPolicyToTheApplication) {
    if (!PlatformLoadsApplicationFonts()) {
        return;
    }
    ASSERT_TRUE(presentation::RegisterBundledTypography());
    presentation::ApplyApplicationTypography();

    const auto &policy = presentation::ApplicationTypographyPolicy();
    const auto font = QApplication::font();
    EXPECT_EQ(font.family(), policy.family);
    EXPECT_DOUBLE_EQ(font.pointSizeF(), policy.bodyPointSize);
    EXPECT_EQ(static_cast<int>(font.weight()), policy.bodyWeight);
}

TEST(TypographyIntegrationTest, ResolvesEveryWeightTheHierarchyUses) {
    if (!PlatformLoadsApplicationFonts()) {
        return;
    }
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
    if (!PlatformLoadsApplicationFonts()) {
        return;
    }
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
