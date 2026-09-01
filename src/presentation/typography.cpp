#include <edit_atlas/presentation/typography.hpp>

#include <QCoreApplication>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QString>
#include <QStringList>
#include <Qt>

#include <spdlog/spdlog.h>

namespace edit_atlas::presentation {
namespace {

const ApplicationTypography kTypography{
    .family = QStringLiteral("Inter"),
    .bodyPointSize = 12,
    .headingPointSize = 14,
    .titlePointSize = 22,
    .bodyWeight = 400,
    .headingWeight = 500,
    .titleWeight = 600,
};

// Registration is attempted once. A second call would add the same faces
// again, and a frontend applying an appearance change must not pay for it.
bool RegisterOnce(void) {
    static const bool registered = [](void) {
        if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()) ==
            nullptr) {
            return false;
        }
        auto usable = false;
        for (const auto &path : BundledTypographyResourcePaths()) {
            if (QFontDatabase::addApplicationFont(path) >= 0) {
                usable = true;
            }
        }
        if (!usable) {
            return false;
        }
        // A face can register while the family stays unresolvable, so the
        // family itself is what decides whether it can be applied.
        return QFontDatabase::families().contains(kTypography.family,
                                                 Qt::CaseInsensitive);
    }();
    return registered;
}

} // namespace

const ApplicationTypography &ApplicationTypographyPolicy(void) {
    return kTypography;
}

QStringList BundledTypographyResourcePaths(void) {
    return {
        QStringLiteral(":/fonts/Inter-Regular.ttf"),
        QStringLiteral(":/fonts/Inter-Medium.ttf"),
        QStringLiteral(":/fonts/Inter-SemiBold.ttf"),
    };
}

bool RegisterBundledTypography(void) { return RegisterOnce(); }

void ApplyApplicationTypography(void) {
    if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()) ==
        nullptr) {
        return;
    }

    auto font = QGuiApplication::font();
    const auto family = ResolvedTypographyFamily();
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    // Point sizing is preserved rather than replaced by pixels so
    // device-independent scaling and high-DPI behavior are unchanged.
    font.setPointSizeF(kTypography.bodyPointSize);
    font.setWeight(static_cast<QFont::Weight>(kTypography.bodyWeight));
    QGuiApplication::setFont(font);

    // Reported like the other resolved runtime facts, so a support bundle
    // says whether the bundled family was applied or the platform one is in
    // use. Nothing else observes typography from outside the process.
    if (family.isEmpty()) {
        SPDLOG_WARN("Interface typeface: platform default "
                    "(bundled family unavailable)");
    } else {
        SPDLOG_INFO("Interface typeface: {}", family.toStdString());
    }
}

QString ResolvedTypographyFamily(void) {
    return RegisterOnce() ? kTypography.family : QString{};
}

} // namespace edit_atlas::presentation
