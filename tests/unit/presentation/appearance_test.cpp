#include <edit_atlas/presentation/appearance.hpp>

#include <QObject>
#include <QSettings>
#include <QString>

#include <gtest/gtest.h>

namespace edit_atlas::presentation {
namespace {

class AppearanceTest : public ::testing::Test {
  protected:
    void SetUp(void) override {
        QSettings settings;
        settings.remove(QStringLiteral("interface/appearance"));
    }
};

TEST_F(AppearanceTest, DefaultsToFollowingTheSystem) {
    const AppearanceController controller;
    EXPECT_EQ(controller.Appearance(), ApplicationAppearance::kSystem);
}

TEST_F(AppearanceTest, PresentsLightWhenNoPlatformSchemeIsAvailable) {
    // Unit tests run without a GUI application, where no platform theme
    // exists to ask, so following the system resolves to the Qt fallback.
    const AppearanceController controller;
    EXPECT_EQ(controller.ResolvedAppearanceValue(),
              ResolvedAppearance::kLight);
}

TEST_F(AppearanceTest, ResolvesAnExplicitSelectionWithoutThePlatform) {
    AppearanceController controller;

    controller.SetAppearance(ApplicationAppearance::kDark);
    EXPECT_EQ(controller.ResolvedAppearanceValue(), ResolvedAppearance::kDark);

    controller.SetAppearance(ApplicationAppearance::kLight);
    EXPECT_EQ(controller.ResolvedAppearanceValue(),
              ResolvedAppearance::kLight);
}

TEST_F(AppearanceTest, PersistsTheSelectionForTheNextLaunch) {
    for (const auto appearance :
         {ApplicationAppearance::kLight, ApplicationAppearance::kDark,
          ApplicationAppearance::kSystem}) {
        {
            AppearanceController controller;
            controller.SetAppearance(appearance);
        }
        const AppearanceController restarted;
        EXPECT_EQ(restarted.Appearance(), appearance);
    }
}

TEST_F(AppearanceTest, TreatsAnUnrecognizedStoredSelectionAsSystem) {
    QSettings settings;
    settings.setValue(QStringLiteral("interface/appearance"),
                      QStringLiteral("solarized"));
    const AppearanceController controller;
    EXPECT_EQ(controller.Appearance(), ApplicationAppearance::kSystem);
}

TEST_F(AppearanceTest, ReportsSelectionAndPresentationSeparately) {
    AppearanceController controller;
    auto selection_changes = 0;
    auto presentation_changes = 0;
    QObject::connect(&controller, &AppearanceController::appearanceChanged,
                     [&selection_changes] { ++selection_changes; });
    QObject::connect(&controller,
                     &AppearanceController::resolvedAppearanceChanged,
                     [&presentation_changes] { ++presentation_changes; });

    controller.SetAppearance(ApplicationAppearance::kDark);
    EXPECT_EQ(selection_changes, 1);
    EXPECT_EQ(presentation_changes, 1);

    // Selecting the same appearance again reports nothing.
    controller.SetAppearance(ApplicationAppearance::kDark);
    EXPECT_EQ(selection_changes, 1);
    EXPECT_EQ(presentation_changes, 1);

    // Without a platform scheme, System and Light present the same palette,
    // so the selection changes while the presentation does not.
    controller.SetAppearance(ApplicationAppearance::kLight);
    EXPECT_EQ(selection_changes, 2);
    EXPECT_EQ(presentation_changes, 2);

    controller.SetAppearance(ApplicationAppearance::kSystem);
    EXPECT_EQ(selection_changes, 3);
    EXPECT_EQ(presentation_changes, 2);
}

TEST_F(AppearanceTest, ExposesThePaletteOfThePresentedAppearance) {
    AppearanceController controller;

    controller.SetAppearance(ApplicationAppearance::kDark);
    EXPECT_EQ(controller.Palette().window,
              AppearancePaletteFor(ResolvedAppearance::kDark).window);

    controller.SetAppearance(ApplicationAppearance::kLight);
    EXPECT_EQ(controller.Palette().window,
              AppearancePaletteFor(ResolvedAppearance::kLight).window);
}

TEST_F(AppearanceTest, RoundTripsEveryAppearanceThroughItsCode) {
    for (const auto appearance :
         {ApplicationAppearance::kSystem, ApplicationAppearance::kLight,
          ApplicationAppearance::kDark}) {
        EXPECT_EQ(AppearanceFromCode(AppearanceCode(appearance)), appearance);
    }
    EXPECT_EQ(AppearanceCode(ApplicationAppearance::kSystem),
              QStringLiteral("system"));
    EXPECT_EQ(AppearanceFromCode(QStringLiteral("solarized")),
              ApplicationAppearance::kSystem);
}

TEST_F(AppearanceTest, PalettesDifferAndEveryColorIsComplete) {
    const auto &dark = AppearancePaletteFor(ResolvedAppearance::kDark);
    const auto &light = AppearancePaletteFor(ResolvedAppearance::kLight);

    EXPECT_NE(dark.window, light.window);
    EXPECT_NE(dark.textPrimary, light.textPrimary);
    EXPECT_NE(dark.accent, light.accent);

    for (const auto *palette : {&dark, &light}) {
        for (const auto &color :
             {palette->accent, palette->accentHovered, palette->accentPressed,
              palette->onAccent, palette->window, palette->surface,
              palette->surfaceAlternate, palette->control,
              palette->controlHovered, palette->controlPressed,
              palette->border, palette->focus, palette->disabled,
              palette->textPrimary, palette->textSecondary,
              palette->textInverted, palette->warning, palette->danger,
              palette->tooltipSurface, palette->tooltipText}) {
            EXPECT_EQ(color.size(), 7) << color.toStdString();
            EXPECT_TRUE(color.startsWith(QLatin1Char('#')))
                << color.toStdString();
        }
    }
}

} // namespace
} // namespace edit_atlas::presentation
