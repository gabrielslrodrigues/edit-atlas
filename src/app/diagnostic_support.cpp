#include <edit_atlas/app/diagnostic_support.hpp>

#include <edit_atlas/app/application_state.hpp>

#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/support/application_logging.hpp>
#include <edit_atlas/support/support_bundle.hpp>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSysInfo>
#include <QtGlobal>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace edit_atlas::app {
namespace {

[[nodiscard]] std::vector<std::string>
FormatIdentifiers(std::vector<core::FormatDescriptor> formats) {
    std::vector<std::string> identifiers;
    identifiers.reserve(formats.size());
    std::ranges::transform(formats, std::back_inserter(identifiers),
                           [](core::FormatDescriptor &format) {
                               return std::move(format.identifier);
                           });
    std::ranges::sort(identifiers);
    return identifiers;
}

[[nodiscard]] std::string Join(const std::vector<std::string> &values) {
    std::string output;
    for (const auto &value : values) {
        if (!output.empty()) {
            output += ",";
        }
        output += value;
    }
    return output;
}

} // namespace

std::filesystem::path ConfiguredLogDirectory(void) {
    return support::ApplicationLogDirectory(
        ConfiguredApplicationDataDirectory());
}

support::DiagnosticEnvironment
CreateDiagnosticEnvironment(const core::FormatRegistry &registry) {
    return support::DiagnosticEnvironment{
        .application_version =
            QCoreApplication::applicationVersion().toStdString(),
        .operating_system = QSysInfo::prettyProductName().toStdString(),
        .architecture = QSysInfo::currentCpuArchitecture().toStdString(),
        .qt_version = qVersion(),
        .platform_plugin = QGuiApplication::platformName().toStdString(),
        .importer_formats = FormatIdentifiers(registry.importer_formats()),
        .exporter_formats = FormatIdentifiers(registry.exporter_formats()),
    };
}

void LogDiagnosticEnvironment(
    const support::DiagnosticEnvironment &environment) {
    SPDLOG_INFO("Operating system: {}", environment.operating_system);
    SPDLOG_INFO("Architecture: {}", environment.architecture);
    SPDLOG_INFO("Qt version: {}", environment.qt_version);
    SPDLOG_INFO("Qt platform plugin: {}", environment.platform_plugin);
    SPDLOG_INFO("Registered import formats: {}",
                Join(environment.importer_formats));
    SPDLOG_INFO("Registered export formats: {}",
                Join(environment.exporter_formats));
}

} // namespace edit_atlas::app
