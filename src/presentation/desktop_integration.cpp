#include <edit_atlas/presentation/desktop_integration.hpp>

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QUrl>

namespace edit_atlas::presentation::desktop_integration {

bool RevealFile(const QString &path) {
#if defined(Q_OS_WIN)
    return QProcess::startDetached(
        QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#elif defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"),
                                   {QStringLiteral("-R"), path});
#else
    return QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo{path}.absolutePath()));
#endif
}

bool OpenDirectory(const QString &path) {
    const QDir directory{path};
    return directory.exists() && QDesktopServices::openUrl(QUrl::fromLocalFile(
                                     directory.absolutePath()));
}

bool OpenExternalUrl(const QUrl &url) {
    const auto scheme = url.scheme();
    if (!url.isValid() || (scheme != QStringLiteral("http") &&
                           scheme != QStringLiteral("https"))) {
        return false;
    }
    return QDesktopServices::openUrl(url);
}

} // namespace edit_atlas::presentation::desktop_integration
