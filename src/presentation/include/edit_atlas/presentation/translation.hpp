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

#ifndef EDIT_ATLAS_PRESENTATION_TRANSLATION_HPP_
#define EDIT_ATLAS_PRESENTATION_TRANSLATION_HPP_

#include "QTranslator"

namespace edit_atlas::presentation {

/// A user-selectable language supported by the application interface.
enum class ApplicationLanguage {
  /// English source strings.
  kEnglish,
  /// Brazilian Portuguese bundled translation.
  kBrazilianPortuguese,
};

/// Returns the persisted language, defaulting to Brazilian Portuguese.
[[nodiscard]] ApplicationLanguage ConfiguredApplicationLanguage(void);

/// Persists the language for subsequent application launches.
void SaveApplicationLanguage(ApplicationLanguage language);

/// Installs the requested bundled translation, or English source text.
[[nodiscard]] bool SetApplicationLanguage(QTranslator& translator,
                                          ApplicationLanguage language);

}  // namespace edit_atlas::presentation

#endif  // EDIT_ATLAS_PRESENTATION_TRANSLATION_HPP_
