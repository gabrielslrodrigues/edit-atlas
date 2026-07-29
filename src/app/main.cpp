#include <edit_atlas/app/application.hpp>
#include <edit_atlas/app/application_style.hpp>
#include <edit_atlas/app/main_window.hpp>
#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/version.hpp>

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QString>
#include <QTranslator>

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <utility>

int main(int argc, char *argv[]) {
    QApplication application{argc, argv};
    edit_atlas::app::ApplyApplicationStyle(application);
    QCoreApplication::setApplicationName(QStringLiteral("Edit Atlas"));
    QCoreApplication::setApplicationVersion(
        QString::fromStdString(std::string{edit_atlas::core::Version()}));
    QCoreApplication::setOrganizationName(QStringLiteral("Edit Atlas"));
    QGuiApplication::setDesktopFileName(QStringLiteral("edit-atlas"));
    QApplication::setWindowIcon(
        QIcon{QStringLiteral(":/icons/edit_atlas.png")});

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    const auto version = std::string{edit_atlas::core::Version()};
    spdlog::info("Starting Edit Atlas {}", version);
    spdlog::info("Using Qt platform {}",
                 QGuiApplication::platformName().toStdString());

    QTranslator translator;
    auto language = edit_atlas::app::ConfiguredApplicationLanguage();
    if (!edit_atlas::app::SetApplicationLanguage(translator, language)) {
        spdlog::warn(
            "Could not load the configured translation; using English");
        language = edit_atlas::app::ApplicationLanguage::kEnglish;
        static_cast<void>(
            edit_atlas::app::SetApplicationLanguage(translator, language));
    }

    auto registry_result = edit_atlas::app::CreateFormatRegistry();
    if (!registry_result.has_value()) {
        spdlog::critical("Could not register built-in formats (error {})",
                         static_cast<int>(registry_result.error()));
        return EXIT_FAILURE;
    }
    auto registry = std::move(*registry_result);
    spdlog::info("Registered {} importer(s) and {} exporter(s)",
                 registry.importer_formats().size(),
                 registry.exporter_formats().size());

    edit_atlas::app::MainWindow window{registry, translator, language};
    window.show();
    return application.exec();
}
