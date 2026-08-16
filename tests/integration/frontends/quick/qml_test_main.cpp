#include <edit_atlas/frontends/quick/application_shell_view_model.hpp>

#include <edit_atlas/presentation/application_state.hpp>
#include <edit_atlas/presentation/translation.hpp>

#include <edit_atlas/core/format_registry.hpp>

#include <edit_atlas/services/built_in_formats.hpp>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QString>
#include <QTemporaryDir>
#include <QTranslator>
#include <QtGlobal>
#include <QtLogging>
#include <QtQuickTest/quicktest.h>
#include <QtTest/qtest.h>

#include <memory>
#include <optional>
#include <utility>

namespace edit_atlas::frontends::quick {
namespace {

class QuickTestSetup final : public QObject {
    Q_OBJECT

  public:
    explicit QuickTestSetup(QObject *parent = nullptr) : QObject{parent} {}

  public slots:
    void applicationAvailable(void) {
        QCoreApplication::setApplicationName(
            QStringLiteral("Edit Atlas Quick QML Tests"));
        QCoreApplication::setOrganizationName(
            QStringLiteral("Edit Atlas Tests"));
        state_root_ = std::make_unique<QTemporaryDir>(QDir::temp().filePath(
            QStringLiteral("edit-atlas-quick-qml-state-XXXXXX")));
        if (!state_root_->isValid()) {
            qFatal("Could not create the Qt Quick test state directory");
        }
        qputenv(presentation::kTestStateRootEnvironment,
                state_root_->path().toUtf8());
        presentation::ConfigureApplicationState();

        auto registry = services::CreateBuiltInFormatRegistry();
        if (!registry.has_value()) {
            qFatal("Could not create the built-in format registry");
        }
        registry_.emplace(std::move(*registry));
        application_shell_ = std::make_unique<ApplicationShellViewModel>(
            *registry_, translator_,
            presentation::ApplicationLanguage::kEnglish);
    }

    void qmlEngineAvailable(QQmlEngine *engine) {
        engine->rootContext()->setContextProperty(
            QStringLiteral("testApplicationShell"), application_shell_.get());
    }

  private:
    std::unique_ptr<QTemporaryDir> state_root_;
    std::optional<core::FormatRegistry> registry_;
    QTranslator translator_;
    std::unique_ptr<ApplicationShellViewModel> application_shell_;
};

} // namespace
} // namespace edit_atlas::frontends::quick

int main(int argc, char *argv[]) {
    QQuickStyle::setFallbackStyle(QStringLiteral("Fusion"));
    QQuickStyle::setStyle(QStringLiteral("EditAtlasStyle"));
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
    QTEST_SET_MAIN_SOURCE_PATH
    edit_atlas::frontends::quick::QuickTestSetup setup;
    return quick_test_main_with_setup(argc, argv, "edit_atlas_quick_qml",
                                      QUICK_TEST_SOURCE_DIR, &setup);
}

#include "qml_test_main.moc"
