#include <QCoreApplication>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QString>
#include <Qt>

#include <cstdlib>

int main(int argc, char *argv[]) {
    QQuickStyle::setFallbackStyle(QStringLiteral("Basic"));
    QQuickStyle::setStyle(QStringLiteral("EditAtlasStyle"));

    QGuiApplication application{argc, argv};
    QCoreApplication::setApplicationName(QStringLiteral("Edit Atlas Quick"));
    QCoreApplication::setOrganizationName(QStringLiteral("Edit Atlas"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("EditAtlas.Frontends.Quick", "Main");

    return application.exec();
}
