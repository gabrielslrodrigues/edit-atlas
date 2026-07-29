#include <edit_atlas/app/application_style.hpp>

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QProxyStyle>
#include <QString>
#include <QStyle>
#include <QStyleHintReturn>
#include <QStyleOption>
#include <QWidget>
#include <Qt>

namespace edit_atlas::app {
namespace {

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

void ApplyApplicationStyle(QApplication &application) {
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
    QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
    application.setStyle(new ResponsiveStyle);

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

    application.setStyleSheet(
        QStringLiteral("QMainWindow {"
                       "  background-color: #181a1f;"
                       "}"
                       "QLabel#subtitleLabel, QLabel#emptyDescriptionLabel,"
                       "QLabel#privacyLabel {"
                       "  color: #9ba1ab;"
                       "}"
                       "QComboBox {"
                       "  background-color: #292c34;"
                       "  border: 1px solid #414650;"
                       "  border-radius: 6px;"
                       "  min-height: 24px;"
                       "  padding: 3px 28px 3px 9px;"
                       "}"
                       "QComboBox:hover {"
                       "  border-color: #707aff;"
                       "}"
                       "QComboBox::drop-down {"
                       "  border: 0;"
                       "  width: 24px;"
                       "}"));
}

} // namespace edit_atlas::app
