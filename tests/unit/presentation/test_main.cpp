#include <edit_atlas/presentation/application_state.hpp>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QTemporaryDir>
#include <QtGlobal>

#include <gtest/gtest.h>

int main(int argc, char *argv[]) {
    QCoreApplication application{argc, argv};
    QCoreApplication::setApplicationName(
        QStringLiteral("Edit Atlas Unit Tests"));
    QCoreApplication::setOrganizationName(QStringLiteral("Edit Atlas Tests"));

    // Persistent state is redirected below a temporary root so settings are
    // isolated from the developer profile, and so QSettings has a store it
    // can actually write to on every platform.
    QTemporaryDir state_root{
        QDir::temp().filePath(QStringLiteral("edit-atlas-state-XXXXXX"))};
    if (!state_root.isValid()) {
        return 1;
    }
    qputenv(edit_atlas::presentation::kTestStateRootEnvironment,
            state_root.path().toUtf8());
    edit_atlas::presentation::ConfigureApplicationState();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
