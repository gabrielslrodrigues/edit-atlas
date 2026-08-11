#include "accessibility.hpp"

#include <edit_atlas/app/application_state.hpp>
#include <edit_atlas/app/application_style.hpp>
#include <edit_atlas/app/diagnostic_support.hpp>
#include <edit_atlas/app/main_window.hpp>
#include <edit_atlas/app/translation.hpp>

#include <edit_atlas/core/version.hpp>

#include <edit_atlas/media/video_decoder.hpp>

#include <edit_atlas/services/built_in_formats.hpp>

#include <edit_atlas/support/application_logging.hpp>

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
    edit_atlas::app::InstallApplicationAccessibility();
    edit_atlas::app::ApplyApplicationStyle(application);
    QCoreApplication::setApplicationName(QStringLiteral("Edit Atlas"));
    QCoreApplication::setApplicationVersion(
        QString::fromStdString(std::string{edit_atlas::core::Version()}));
    QCoreApplication::setOrganizationName(QStringLiteral("Edit Atlas"));
    edit_atlas::app::ConfigureApplicationState();
    QGuiApplication::setDesktopFileName(QStringLiteral("edit-atlas"));
    QApplication::setWindowIcon(
        QIcon{QStringLiteral(":/icons/edit_atlas.png")});

    const auto log_directory = edit_atlas::app::ConfiguredLogDirectory();
    const auto logging_result =
        edit_atlas::support::InitializeApplicationLogging(
            edit_atlas::support::LoggingOptions{
                .directory = log_directory,
                .maximum_file_size =
                    edit_atlas::support::kDefaultMaximumLogFileSize,
                .maximum_files = edit_atlas::support::kDefaultMaximumLogFiles,
                .maximum_age = edit_atlas::support::kDefaultLogRetention,
            });
    if (!logging_result.has_value()) {
        SPDLOG_WARN("Persistent logging is unavailable");
    }

    const auto version = std::string{edit_atlas::core::Version()};
    SPDLOG_INFO("Starting Edit Atlas {}", version);
    const auto video_backend = edit_atlas::media::GetVideoBackendInformation();
    SPDLOG_INFO("Video backend: {} {}", video_backend.name,
                video_backend.version);

    QTranslator translator;
    auto language = edit_atlas::app::ConfiguredApplicationLanguage();
    if (!edit_atlas::app::SetApplicationLanguage(translator, language)) {
        SPDLOG_WARN("Could not load the configured translation; using English");
        language = edit_atlas::app::ApplicationLanguage::kEnglish;
        static_cast<void>(
            edit_atlas::app::SetApplicationLanguage(translator, language));
    }

    auto registry_result = edit_atlas::services::CreateBuiltInFormatRegistry();
    if (!registry_result.has_value()) {
        SPDLOG_CRITICAL("Could not register built-in formats (error {})",
                        static_cast<int>(registry_result.error()));
        return EXIT_FAILURE;
    }
    auto registry = std::move(*registry_result);
    const auto diagnostic_environment =
        edit_atlas::app::CreateDiagnosticEnvironment(registry);
    edit_atlas::app::LogDiagnosticEnvironment(diagnostic_environment);

    edit_atlas::app::MainWindow window{
        registry, translator, language, log_directory, diagnostic_environment,
    };
    window.show();
    return application.exec();
}
