#include <edit_atlas/frontends/widgets/application_style.hpp>

#include <edit_atlas/presentation/appearance.hpp>

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QIODevice>
#include <QPalette>
#include <QProxyStyle>
#include <QResource>
#include <QString>
#include <QStyle>
#include <QStyleHintReturn>
#include <QStyleOption>
#include <QWidget>
#include <Qt>

#include <utility>
#include <vector>

static void InitializeApplicationStyleResources(void) {
    Q_INIT_RESOURCE(edit_atlas_widgets_frontend_style);
}

namespace edit_atlas::frontends::widgets {
namespace {

constexpr auto kStyleSheetResourcePath = ":/styles/edit_atlas.qss";

class ResponsiveStyle final : public QProxyStyle {
  public:
    ResponsiveStyle(void) : QProxyStyle(QStringLiteral("Fusion")) {}
    ~ResponsiveStyle(void) override = default;

    [[nodiscard]] int
    styleHint(StyleHint hint, const QStyleOption *option = nullptr,
              const QWidget *widget = nullptr,
              QStyleHintReturn *return_data = nullptr) const override {
        switch (hint) {
        case QStyle::SH_Widget_Animate:
        case QStyle::SH_Widget_Animation_Duration:
            return 0;
        default:
            return QProxyStyle::styleHint(hint, option, widget, return_data);
        }
    }
};

std::vector<std::pair<QString, QString>>
StyleSheetTokens(const presentation::AppearancePalette &p) {
    return {
        {QStringLiteral("accent"), p.accent},
        {QStringLiteral("border"), p.border},
        {QStringLiteral("control"), p.control},
        {QStringLiteral("controlPressed"), p.controlPressed},
        {QStringLiteral("danger"), p.danger},
        {QStringLiteral("disabled"), p.disabled},
        {QStringLiteral("focus"), p.focus},
        {QStringLiteral("surface"), p.surface},
        {QStringLiteral("surfaceAlternate"), p.surfaceAlternate},
        {QStringLiteral("textSecondary"), p.textSecondary},
        {QStringLiteral("warning"), p.warning},
        {QStringLiteral("window"), p.window},
    };
}

} // namespace

QString
LoadApplicationStyleSheet(const presentation::AppearancePalette &palette) {
    static const bool resources_initialized = [](void) {
        ::InitializeApplicationStyleResources();
        return true;
    }();
    static_cast<void>(resources_initialized);

    QFile style_sheet{QString::fromLatin1(kStyleSheetResourcePath)};
    if (!style_sheet.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not load application stylesheet resource"
                   << kStyleSheetResourcePath << ':'
                   << style_sheet.errorString();
        return {};
    }

    auto contents = QString::fromUtf8(style_sheet.readAll());
    for (const auto &[token, color] : StyleSheetTokens(palette)) {
        contents.replace(u'@' + token + u'@', color);
    }
    return contents;
}

QPalette BuildApplicationPalette(const presentation::AppearancePalette &p) {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor{p.window});
    palette.setColor(QPalette::WindowText, QColor{p.textPrimary});
    palette.setColor(QPalette::Base, QColor{p.surface});
    palette.setColor(QPalette::AlternateBase, QColor{p.surfaceAlternate});
    palette.setColor(QPalette::ToolTipBase, QColor{p.tooltipSurface});
    palette.setColor(QPalette::ToolTipText, QColor{p.tooltipText});
    palette.setColor(QPalette::Text, QColor{p.textPrimary});
    palette.setColor(QPalette::Button, QColor{p.control});
    palette.setColor(QPalette::ButtonText, QColor{p.textPrimary});
    palette.setColor(QPalette::BrightText, QColor{p.textInverted});
    palette.setColor(QPalette::Highlight, QColor{p.accent});
    palette.setColor(QPalette::HighlightedText, QColor{p.onAccent});
    palette.setColor(QPalette::PlaceholderText, QColor{p.textSecondary});
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor{p.disabled});
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor{p.disabled});
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor{p.disabled});
    return palette;
}

void ApplyApplicationAppearance(
    QApplication &application,
    const presentation::AppearancePalette &palette) {
    application.setPalette(BuildApplicationPalette(palette));
    application.setStyleSheet(LoadApplicationStyleSheet(palette));
}

void ApplyApplicationStyle(QApplication &application,
                           const presentation::AppearancePalette &palette) {
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
    application.setStyle(new ResponsiveStyle);

    auto interface_font = application.font();
    if (interface_font.pointSizeF() < 12.0) {
        interface_font.setPointSizeF(12.0);
        application.setFont(interface_font);
    }

    ApplyApplicationAppearance(application, palette);
}

} // namespace edit_atlas::frontends::widgets
