#include <edit_atlas/presentation/application_state.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QTemporaryDir>
#include <QtGlobal>

#include <gtest/gtest.h>

int main(int argc, char *argv[]) {
    QCoreApplication application{argc, argv};
    QCoreApplication::setApplicationName(
        QStringLiteral("Edit Atlas Quick Integration Tests"));
    QCoreApplication::setOrganizationName(QStringLiteral("Edit Atlas Tests"));

    QTemporaryDir state_root{
        QDir::temp().filePath(QStringLiteral("edit-atlas-quick-state-XXXXXX"))};
    if (!state_root.isValid()) {
        return 1;
    }
    qputenv(edit_atlas::presentation::kTestStateRootEnvironment,
            state_root.path().toUtf8());
    edit_atlas::presentation::ConfigureApplicationState();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
