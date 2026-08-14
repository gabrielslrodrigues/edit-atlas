#ifndef EDIT_ATLAS_PRESENTATION_TRANSLATION_HPP_
#define EDIT_ATLAS_PRESENTATION_TRANSLATION_HPP_

class QTranslator;

namespace edit_atlas::presentation {

/// A user-selectable language supported by the application interface.
enum class ApplicationLanguage {
    kEnglish,
    kBrazilianPortuguese,
};

/// Returns the persisted language, defaulting to Brazilian Portuguese.
[[nodiscard]] ApplicationLanguage ConfiguredApplicationLanguage(void);

/// Persists the language for subsequent application launches.
void SaveApplicationLanguage(ApplicationLanguage language);

/// Installs the requested bundled translation, or English source text.
[[nodiscard]] bool SetApplicationLanguage(QTranslator &translator,
                                          ApplicationLanguage language);

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TRANSLATION_HPP_
