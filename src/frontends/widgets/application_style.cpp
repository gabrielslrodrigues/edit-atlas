#include <edit_atlas/frontends/widgets/application_style.hpp>

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

} // namespace

QString LoadApplicationStyleSheet(void) {
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
    return QString::fromUtf8(style_sheet.readAll());
}

void ApplyApplicationStyle(QApplication &application) {
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
    application.setStyle(new ResponsiveStyle);

    auto interface_font = application.font();
    if (interface_font.pointSizeF() < 12.0) {
        interface_font.setPointSizeF(12.0);
        application.setFont(interface_font);
    }

    QPalette palette;
    palette.setColor(QPalette::Window, QColor{24, 26, 31});
    palette.setColor(QPalette::WindowText, QColor{232, 234, 237});
    palette.setColor(QPalette::Base, QColor{18, 20, 24});
    palette.setColor(QPalette::AlternateBase, QColor{31, 34, 40});
    palette.setColor(QPalette::ToolTipBase, QColor{38, 41, 48});
    palette.setColor(QPalette::ToolTipText, QColor{242, 243, 245});
    palette.setColor(QPalette::Text, QColor{232, 234, 237});
    palette.setColor(QPalette::Button, QColor{37, 40, 47});
    palette.setColor(QPalette::ButtonText, QColor{232, 234, 237});
    palette.setColor(QPalette::BrightText, QColor{255, 255, 255});
    palette.setColor(QPalette::Highlight, QColor{112, 122, 255});
    palette.setColor(QPalette::HighlightedText, QColor{255, 255, 255});
    palette.setColor(QPalette::PlaceholderText, QColor{139, 145, 156});
    application.setPalette(palette);

    application.setStyleSheet(LoadApplicationStyleSheet());
}

} // namespace edit_atlas::frontends::widgets
