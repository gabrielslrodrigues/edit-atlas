#include <edit_atlas/app/application_state.hpp>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QtGlobal>

#include <gtest/gtest.h>

int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
#if defined(Q_OS_MACOS)
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("minimal"));
#elif defined(Q_OS_WIN)
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("windows"));
#elif defined(Q_OS_LINUX)
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
#else
#error "Unsupported Qt integration-test platform"
#endif
    }
    QApplication application{argc, argv};
    QCoreApplication::setApplicationName(
        QStringLiteral("Edit Atlas Integration Tests"));
    QCoreApplication::setOrganizationName(QStringLiteral("Edit Atlas Tests"));

    QTemporaryDir state_root{
        QDir::temp().filePath(QStringLiteral("edit-atlas-state-XXXXXX"))};
    if (!state_root.isValid()) {
        return 1;
    }
    qputenv(edit_atlas::app::kTestStateRootEnvironment,
            state_root.path().toUtf8());
    edit_atlas::app::ConfigureApplicationState();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
