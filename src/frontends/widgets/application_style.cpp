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

#include "edit_atlas/frontends/widgets/application_style.hpp"

#include <utility>
#include <vector>

#include "QApplication"
#include "QColor"
#include "QDebug"
#include "QFile"
#include "QFont"
#include "QIODevice"
#include "QPalette"
#include "QProxyStyle"
#include "QResource"
#include "QString"
#include "QStyle"
#include "QStyleHintReturn"
#include "QStyleOption"
#include "QWidget"
#include "Qt"

#include "edit_atlas/presentation/appearance.hpp"
#include "edit_atlas/presentation/typography.hpp"

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

  [[nodiscard]] int styleHint(
      StyleHint hint, const QStyleOption* option = nullptr,
      const QWidget* widget = nullptr,
      QStyleHintReturn* return_data = nullptr) const override {
    switch (hint) {
      case QStyle::SH_Widget_Animate:
      case QStyle::SH_Widget_Animation_Duration:
        return 0;
      default:
        return QProxyStyle::styleHint(hint, option, widget, return_data);
    }
  }
};

std::vector<std::pair<QString, QString>> StyleSheetTokens(
    const presentation::AppearancePalette& p) {
  return {
      {QStringLiteral("accent"), p.accent},
      {QStringLiteral("border"), p.border},
      {QStringLiteral("control"), p.control},
      {QStringLiteral("controlPressed"), p.control_pressed},
      {QStringLiteral("danger"), p.danger},
      {QStringLiteral("disabled"), p.disabled},
      {QStringLiteral("focus"), p.focus},
      {QStringLiteral("surface"), p.surface},
      {QStringLiteral("surfaceAlternate"), p.surface_alternate},
      {QStringLiteral("textSecondary"), p.text_secondary},
      {QStringLiteral("warning"), p.warning},
      {QStringLiteral("window"), p.window},
      {QStringLiteral("bodyWeight"),
       QString::number(
           presentation::ApplicationTypographyPolicy().body_weight)},
      {QStringLiteral("headingWeight"),
       QString::number(
           presentation::ApplicationTypographyPolicy().heading_weight)},
      {QStringLiteral("titleWeight"),
       QString::number(
           presentation::ApplicationTypographyPolicy().title_weight)},
  };
}

}  // namespace

QString LoadApplicationStyleSheet(
    const presentation::AppearancePalette& palette) {
  static const bool kResourcesInitialized = [](void) {
    ::InitializeApplicationStyleResources();
    return true;
  }();
  static_cast<void>(kResourcesInitialized);

  QFile style_sheet{QString::fromLatin1(kStyleSheetResourcePath)};
  if (!style_sheet.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "Could not load application stylesheet resource"
               << kStyleSheetResourcePath << ':' << style_sheet.errorString();
    return {};
  }

  auto contents = QString::fromUtf8(style_sheet.readAll());
  for (const auto& [token, color] : StyleSheetTokens(palette)) {
    contents.replace(u'@' + token + u'@', color);
  }
  return contents;
}

QPalette BuildApplicationPalette(const presentation::AppearancePalette& p) {
  QPalette palette;
  palette.setColor(QPalette::Window, QColor{p.window});
  palette.setColor(QPalette::WindowText, QColor{p.text_primary});
  palette.setColor(QPalette::Base, QColor{p.surface});
  palette.setColor(QPalette::AlternateBase, QColor{p.surface_alternate});
  palette.setColor(QPalette::ToolTipBase, QColor{p.tooltip_surface});
  palette.setColor(QPalette::ToolTipText, QColor{p.tooltip_text});
  palette.setColor(QPalette::Text, QColor{p.text_primary});
  palette.setColor(QPalette::Button, QColor{p.control});
  palette.setColor(QPalette::ButtonText, QColor{p.text_primary});
  palette.setColor(QPalette::BrightText, QColor{p.text_inverted});
  palette.setColor(QPalette::Highlight, QColor{p.accent});
  palette.setColor(QPalette::HighlightedText, QColor{p.on_accent});
  palette.setColor(QPalette::PlaceholderText, QColor{p.text_secondary});
  palette.setColor(QPalette::Disabled, QPalette::Text, QColor{p.disabled});
  palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                   QColor{p.disabled});
  palette.setColor(QPalette::Disabled, QPalette::WindowText,
                   QColor{p.disabled});
  return palette;
}

void ApplyApplicationAppearance(
    QApplication& application, const presentation::AppearancePalette& palette) {
  application.setPalette(BuildApplicationPalette(palette));
  application.setStyleSheet(LoadApplicationStyleSheet(palette));
}

void ApplyApplicationStyle(QApplication& application,
                           const presentation::AppearancePalette& palette) {
  QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
  QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
  QApplication::setEffectEnabled(Qt::UI_FadeTooltip, false);
  application.setStyle(new ResponsiveStyle);

  // Family, size, and weight come from the shared policy, which also
  // registers the bundled faces and falls back to the platform family.
  presentation::ApplyApplicationTypography();

  ApplyApplicationAppearance(application, palette);
}

}  // namespace edit_atlas::frontends::widgets
