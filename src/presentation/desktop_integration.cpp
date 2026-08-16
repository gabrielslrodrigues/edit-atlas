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

} // namespace edit_atlas::presentation::desktop_integration
