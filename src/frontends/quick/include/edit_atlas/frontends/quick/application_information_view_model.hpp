#ifndef EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_INFORMATION_VIEW_MODEL_HPP_
#define EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_INFORMATION_VIEW_MODEL_HPP_

#include <edit_atlas/core/format_registry.hpp>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace edit_atlas::frontends::quick {

/// Exposes runtime, diagnostic, and licensing information to Qt Quick.
class ApplicationInformationViewModel final : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ApplicationInformation)
    QML_UNCREATABLE("Provided by the Edit Atlas application")

    Q_PROPERTY(QString applicationVersion READ ApplicationVersion CONSTANT)
    Q_PROPERTY(QString operatingSystem READ OperatingSystem CONSTANT)
    Q_PROPERTY(QString architecture READ Architecture CONSTANT)
    Q_PROPERTY(QString qtVersion READ QtVersion CONSTANT)
    Q_PROPERTY(QString platformPlugin READ PlatformPlugin CONSTANT)
    Q_PROPERTY(QString videoBackendName READ VideoBackendName CONSTANT)
    Q_PROPERTY(QString videoBackendVersion READ VideoBackendVersion CONSTANT)
    Q_PROPERTY(QString videoBackendLicense READ VideoBackendLicense CONSTANT)
    Q_PROPERTY(QString videoBackendConfiguration READ VideoBackendConfiguration
                   CONSTANT)
    Q_PROPERTY(QStringList importFormats READ ImportFormats CONSTANT)
    Q_PROPERTY(QStringList exportFormats READ ExportFormats CONSTANT)
    Q_PROPERTY(QString logDirectory READ LogDirectory CONSTANT)

  public:
    /// Collects immutable runtime information for the registered application.
    explicit ApplicationInformationViewModel(
        const core::FormatRegistry &registry, QObject *parent = nullptr);
    /// Destroys the immutable information adapter.
    ~ApplicationInformationViewModel(void) override = default;

    /// Information adapters are non-copyable QObject owners.
    ApplicationInformationViewModel(const ApplicationInformationViewModel &) =
        delete;
    /// Information adapters are non-copy-assignable QObject owners.
    ApplicationInformationViewModel &
    operator=(const ApplicationInformationViewModel &) = delete;
    /// Information adapters are non-movable QObject owners.
    ApplicationInformationViewModel(ApplicationInformationViewModel &&) =
        delete;
    /// Information adapters are non-move-assignable QObject owners.
    ApplicationInformationViewModel &
    operator=(ApplicationInformationViewModel &&) = delete;

    /// Returns the Edit Atlas version.
    [[nodiscard]] QString ApplicationVersion(void) const;
    /// Returns the operating-system description reported by Qt.
    [[nodiscard]] QString OperatingSystem(void) const;
    /// Returns the runtime CPU architecture reported by Qt.
    [[nodiscard]] QString Architecture(void) const;
    /// Returns the linked Qt runtime version.
    [[nodiscard]] QString QtVersion(void) const;
    /// Returns the active Qt platform plugin.
    [[nodiscard]] QString PlatformPlugin(void) const;
    /// Returns the linked video backend name.
    [[nodiscard]] QString VideoBackendName(void) const;
    /// Returns the linked video backend version.
    [[nodiscard]] QString VideoBackendVersion(void) const;
    /// Returns the license reported by the linked video backend.
    [[nodiscard]] QString VideoBackendLicense(void) const;
    /// Returns the linked video backend build configuration.
    [[nodiscard]] QString VideoBackendConfiguration(void) const;
    /// Returns stable identifiers of registered import formats.
    [[nodiscard]] QStringList ImportFormats(void) const;
    /// Returns stable identifiers of registered export formats.
    [[nodiscard]] QStringList ExportFormats(void) const;
    /// Returns the configured private application log directory.
    [[nodiscard]] QString LogDirectory(void) const;

    /// Opens the configured log directory in the platform file manager.
    Q_INVOKABLE bool OpenLogDirectory(void) const;
    /// Opens the Edit Atlas project website.
    Q_INVOKABLE bool OpenProjectWebsite(void) const;
    /// Opens Qt's open-source licensing information.
    Q_INVOKABLE bool OpenQtLicensingInformation(void) const;
    /// Opens FFmpeg's legal and licensing information.
    Q_INVOKABLE bool OpenFfmpegLegalInformation(void) const;

  private:
    QString application_version_;
    QString operating_system_;
    QString architecture_;
    QString qt_version_;
    QString platform_plugin_;
    QString video_backend_name_;
    QString video_backend_version_;
    QString video_backend_license_;
    QString video_backend_configuration_;
    QStringList import_formats_;
    QStringList export_formats_;
    QString log_directory_;
};

} // namespace edit_atlas::frontends::quick

#endif // EDIT_ATLAS_FRONTENDS_QUICK_APPLICATION_INFORMATION_VIEW_MODEL_HPP_
