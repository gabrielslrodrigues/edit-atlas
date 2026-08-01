#include <edit_atlas/app/application_style.hpp>

#include <QApplication>
#include <QColor>
#include <QFont>
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

    application.setStyleSheet(
        QStringLiteral("QMainWindow {"
                       "  background-color: #181a1f;"
                       "}"
                       "QLabel#subtitleLabel, QLabel#emptyDescriptionLabel,"
                       "QLabel#privacyLabel, QLabel#filterResultLabel {"
                       "  color: #9ba1ab;"
                       "}"
                       "QLabel#filterErrorLabel {"
                       "  color: #ff8a80;"
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
                       "}"
                       "QLineEdit, QPushButton, QToolButton {"
                       "  background-color: #25282f;"
                       "  border: 1px solid #414650;"
                       "  border-radius: 6px;"
                       "  min-height: 28px;"
                       "  padding: 3px 9px;"
                       "}"
                       "QLineEdit:focus, QPushButton:hover, QToolButton:hover,"
                       "QToolButton:checked {"
                       "  border-color: #707aff;"
                       "}"
                       "QPushButton:pressed, QToolButton:pressed {"
                       "  background-color: #30343d;"
                       "}"
                       "QComboBox:disabled, QLineEdit:disabled,"
                       "QPushButton:disabled, QToolButton:disabled {"
                       "  background-color: #202228;"
                       "  border-color: #30343d;"
                       "  color: #686e78;"
                       "}"
                       "QTableView, QTreeWidget {"
                       "  border: 1px solid #353a44;"
                       "  border-radius: 6px;"
                       "  gridline-color: #30343d;"
                       "  selection-background-color: #5058b8;"
                       "}"
                       "QHeaderView::section {"
                       "  background-color: #25282f;"
                       "  border: 0;"
                       "  border-bottom: 1px solid #414650;"
                       "  padding: 6px 8px;"
                       "}"
                       "QGroupBox {"
                       "  border: 1px solid #353a44;"
                       "  border-radius: 6px;"
                       "  margin-top: 8px;"
                       "  padding-top: 8px;"
                       "}"
                       "QGroupBox::title {"
                       "  left: 8px;"
                       "  padding: 0 4px;"
                       "}"
                       "QProgressBar {"
                       "  background-color: #25282f;"
                       "  border: 1px solid #414650;"
                       "  border-radius: 4px;"
                       "  max-height: 8px;"
                       "}"));
}

} // namespace edit_atlas::app
