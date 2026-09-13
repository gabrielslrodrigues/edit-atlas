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

#include "edit_atlas/presentation/translation.hpp"

#include "QCoreApplication"
#include "QResource"
#include "QSettings"
#include "QString"
#include "QTranslator"
#include "QVariant"

static void InitializeTranslationResources(void) {
  Q_INIT_RESOURCE(edit_atlas_presentation_translations);
}

namespace edit_atlas::presentation {

ApplicationLanguage ConfiguredApplicationLanguage(void) {
  const QSettings settings;
  const auto code =
      settings
          .value(QStringLiteral("interface/language"), QStringLiteral("pt_BR"))
          .toString();
  return code == QStringLiteral("en")
             ? ApplicationLanguage::kEnglish
             : ApplicationLanguage::kBrazilianPortuguese;
}

void SaveApplicationLanguage(ApplicationLanguage language) {
  QSettings settings;
  settings.setValue(QStringLiteral("interface/language"),
                    language == ApplicationLanguage::kEnglish
                        ? QStringLiteral("en")
                        : QStringLiteral("pt_BR"));
}

bool SetApplicationLanguage(QTranslator& translator,
                            ApplicationLanguage language) {
  static const bool kResourcesInitialized = [](void) {
    InitializeTranslationResources();
    return true;
  }();
  static_cast<void>(kResourcesInitialized);

  static_cast<void>(QCoreApplication::removeTranslator(&translator));
  if (language == ApplicationLanguage::kEnglish) {
    return true;
  }

  if (!translator.load(QStringLiteral(":/i18n/edit_atlas_pt_BR.qm"))) {
    return false;
  }
  return QCoreApplication::installTranslator(&translator);
}

}  // namespace edit_atlas::presentation
