#include <edit_atlas/app/translation.hpp>

#include <QCoreApplication>
#include <QResource>
#include <QSettings>
#include <QString>
#include <QTranslator>
#include <QVariant>

static void InitializeTranslationResources(void) {
    Q_INIT_RESOURCE(edit_atlas_app_translations);
}

namespace edit_atlas::app {

ApplicationLanguage ConfiguredApplicationLanguage(void) {
    const QSettings settings;
    const auto code = settings
                          .value(QStringLiteral("interface/language"),
                                 QStringLiteral("pt_BR"))
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

bool SetApplicationLanguage(QTranslator &translator,
                            ApplicationLanguage language) {
    static const bool resources_initialized = [](void) {
        InitializeTranslationResources();
        return true;
    }();
    static_cast<void>(resources_initialized);

    static_cast<void>(QCoreApplication::removeTranslator(&translator));
    if (language == ApplicationLanguage::kEnglish) {
        return true;
    }

    if (!translator.load(QStringLiteral(":/i18n/edit_atlas_pt_BR.qm"))) {
        return false;
    }
    return QCoreApplication::installTranslator(&translator);
}

} // namespace edit_atlas::app
