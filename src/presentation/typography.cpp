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

#include "edit_atlas/presentation/typography.hpp"

#include "QCoreApplication"
#include "QFont"
#include "QFontDatabase"
#include "QGuiApplication"
#include "QString"
#include "QStringList"
#include "Qt"
#include "spdlog/spdlog.h"

namespace edit_atlas::presentation {
namespace {

// Registration is attempted once. A second call would add the same faces
// again, and a frontend applying an appearance change must not pay for it.
bool RegisterOnce(void) {
  static const bool kRegistered = [](void) {
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) ==
        nullptr) {
      return false;
    }
    auto usable = false;
    for (const auto& path : BundledTypographyResourcePaths()) {
      if (QFontDatabase::addApplicationFont(path) >= 0) {
        usable = true;
      }
    }
    if (!usable) {
      return false;
    }
    // A face can register while the family stays unresolvable, so the
    // family itself is what decides whether it can be applied.
    return QFontDatabase::families().contains(
        ApplicationTypographyPolicy().family, Qt::CaseInsensitive);
  }();
  return kRegistered;
}

}  // namespace

const ApplicationTypography& ApplicationTypographyPolicy(void) {
  // Retain the shared QString data through process teardown.
  static const auto& kTypography = *new const ApplicationTypography{
      .family = QStringLiteral("Inter"),
      .body_point_size = 12,
      .heading_point_size = 14,
      .title_point_size = 22,
      .body_weight = 400,
      .heading_weight = 500,
      .title_weight = 600,
  };
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
  if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
    return;
  }

  auto font = QGuiApplication::font();
  const auto family = ResolvedTypographyFamily();
  if (!family.isEmpty()) {
    font.setFamily(family);
  }
  // Point sizing is preserved rather than replaced by pixels so
  // device-independent scaling and high-DPI behavior are unchanged.
  font.setPointSizeF(ApplicationTypographyPolicy().body_point_size);
  font.setWeight(
      static_cast<QFont::Weight>(ApplicationTypographyPolicy().body_weight));
  QGuiApplication::setFont(font);

  // Reported like the other resolved runtime facts, so a support bundle
  // says whether the bundled family was applied or the platform one is in
  // use. Nothing else observes typography from outside the process.
  if (family.isEmpty()) {
    SPDLOG_WARN(
        "Interface typeface: platform default "
        "(bundled family unavailable)");
  } else {
    SPDLOG_INFO("Interface typeface: {}", family.toStdString());
  }
}

QString ResolvedTypographyFamily(void) {
  return RegisterOnce() ? ApplicationTypographyPolicy().family : QString{};
}

}  // namespace edit_atlas::presentation
