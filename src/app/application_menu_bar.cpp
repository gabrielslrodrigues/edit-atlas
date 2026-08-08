#include <edit_atlas/app/application_menu_bar.hpp>

#include "accessibility.hpp"

#include <QAction>
#include <QComboBox>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QSettings>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QWidget>
#include <Qt>

namespace edit_atlas::app {
namespace {

constexpr qsizetype kMaximumRecentFiles = 10;

} // namespace

ApplicationMenuBar::ApplicationMenuBar(ApplicationLanguage initial_language,
                                       QWidget *parent)
    : QMenuBar{parent} {
    BuildUi(initial_language);
    RetranslateUi();
}

void ApplicationMenuBar::RememberRecentFile(const QString &path) {
    if (!remember_recent_action_->isChecked()) {
        return;
    }

    QSettings settings;
    auto recent_files =
        settings.value(QStringLiteral("files/recent")).toStringList();
    recent_files.removeAll(path);
    recent_files.prepend(path);
    while (recent_files.size() > kMaximumRecentFiles) {
        recent_files.removeLast();
    }
    settings.setValue(QStringLiteral("files/recent"), recent_files);
    UpdateRecentFilesMenu();
}

void ApplicationMenuBar::RetranslateUi(void) {
    file_menu_->setTitle(tr("&File"));
    open_action_->setText(tr("&Open Timeline…"));
    recent_files_menu_->setTitle(tr("Open &Recent"));
    remember_recent_action_->setText(tr("Remember Recent Files"));
    remember_recent_action_->setToolTip(
        tr("When enabled, Edit Atlas stores only the paths of opened files."));
    export_action_->setText(tr("&Export Spreadsheet"));
    export_action_->setToolTip(
        tr("Export the open timeline as an Excel workbook."));
    exit_action_->setText(tr("E&xit"));
    help_menu_->setTitle(tr("&Help"));
    export_logs_action_->setText(tr("Export Diagnostic &Logs"));
    export_logs_action_->setToolTip(
        tr("Create a local support bundle containing diagnostic logs."));
    about_action_->setText(tr("&About Edit Atlas"));
    language_selector_->setToolTip(tr("Language"));
    language_selector_->setAccessibleName(tr("Language"));
    UpdateRecentFilesMenu();
}

void ApplicationMenuBar::SetBusy(bool busy) {
    busy_ = busy;
    UpdateActions();
}

void ApplicationMenuBar::SetDocumentAvailable(bool available) {
    document_available_ = available;
    UpdateActions();
}

void ApplicationMenuBar::SetExportAvailable(bool available) {
    export_available_ = available;
    UpdateActions();
}

void ApplicationMenuBar::SetLanguage(ApplicationLanguage language) {
    const QSignalBlocker blocker{language_selector_};
    language_selector_->setCurrentIndex(
        language_selector_->findData(static_cast<int>(language)));
}

void ApplicationMenuBar::BuildUi(ApplicationLanguage initial_language) {
    file_menu_ = addMenu(QString{});

    open_action_ = file_menu_->addAction(QString{});
    open_action_->setShortcut(QKeySequence::Open);
    connect(open_action_, &QAction::triggered, this,
            &ApplicationMenuBar::OpenRequested);

    recent_files_menu_ = file_menu_->addMenu(QString{});

    remember_recent_action_ = file_menu_->addAction(QString{});
    remember_recent_action_->setCheckable(true);
    const QSettings settings;
    remember_recent_action_->setChecked(
        settings.value(QStringLiteral("files/rememberRecent"), false).toBool());
    connect(remember_recent_action_, &QAction::toggled, this,
            &ApplicationMenuBar::SetRememberRecentFiles);

    file_menu_->addSeparator();
    export_action_ = file_menu_->addAction(QString{});
    connect(export_action_, &QAction::triggered, this,
            &ApplicationMenuBar::ExportSpreadsheetRequested);

    file_menu_->addSeparator();
    exit_action_ = file_menu_->addAction(QString{});
    exit_action_->setShortcut(QKeySequence::Quit);
    connect(exit_action_, &QAction::triggered, this,
            &ApplicationMenuBar::ExitRequested);

    help_menu_ = addMenu(QString{});
    export_logs_action_ = help_menu_->addAction(QString{});
    connect(export_logs_action_, &QAction::triggered, this,
            &ApplicationMenuBar::ExportDiagnosticLogsRequested);
    help_menu_->addSeparator();
    about_action_ = help_menu_->addAction(QString{});
    connect(about_action_, &QAction::triggered, this,
            &ApplicationMenuBar::AboutRequested);

    language_selector_ = new QComboBox{this};
    language_selector_->addItem(
        QStringLiteral("Português (Brasil)"),
        static_cast<int>(ApplicationLanguage::kBrazilianPortuguese));
    language_selector_->addItem(
        QStringLiteral("English"),
        static_cast<int>(ApplicationLanguage::kEnglish));
    language_selector_->setMinimumContentsLength(18);
    connect(language_selector_, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                const auto language = static_cast<ApplicationLanguage>(
                    language_selector_->itemData(index).toInt());
                emit LanguageSelected(language);
            });

    SetAutomationIdentifier(*this, u"applicationMenuBar");
    SetAutomationIdentifier(*file_menu_, u"fileMenu");
    SetAutomationIdentifier(*open_action_, u"openDocumentAction");
    SetAutomationIdentifier(*recent_files_menu_, u"recentFilesMenu");
    SetAutomationIdentifier(*remember_recent_action_,
                            u"rememberRecentFilesAction");
    SetAutomationIdentifier(*export_action_, u"exportAction");
    SetAutomationIdentifier(*exit_action_, u"exitAction");
    SetAutomationIdentifier(*help_menu_, u"helpMenu");
    SetAutomationIdentifier(*export_logs_action_,
                            u"exportDiagnosticLogsAction");
    SetAutomationIdentifier(*about_action_, u"aboutAction");
    SetAutomationIdentifier(*language_selector_, u"languageSelector");

    setCornerWidget(language_selector_, Qt::TopRightCorner);
    SetLanguage(initial_language);
    UpdateActions();
}

void ApplicationMenuBar::SetRememberRecentFiles(bool enabled) {
    QSettings settings;
    settings.setValue(QStringLiteral("files/rememberRecent"), enabled);
    if (!enabled) {
        settings.remove(QStringLiteral("files/recent"));
    }
    UpdateRecentFilesMenu();
}

void ApplicationMenuBar::UpdateActions(void) {
    open_action_->setEnabled(!busy_);
    recent_files_menu_->setEnabled(!busy_);
    export_action_->setEnabled(!busy_ && document_available_ &&
                               export_available_);
    export_logs_action_->setEnabled(!busy_);
}

void ApplicationMenuBar::UpdateRecentFilesMenu(void) {
    recent_files_menu_->clear();
    if (!remember_recent_action_->isChecked()) {
        auto *disabled = recent_files_menu_->addAction(tr("History disabled"));
        disabled->setEnabled(false);
        return;
    }

    const QSettings settings;
    const auto recent_files =
        settings.value(QStringLiteral("files/recent")).toStringList();
    if (recent_files.empty()) {
        auto *empty = recent_files_menu_->addAction(tr("No recent files"));
        empty->setEnabled(false);
        return;
    }

    qsizetype index = 0;
    for (const auto &path : recent_files) {
        auto *action =
            recent_files_menu_->addAction(QFileInfo{path}.fileName());
        SetAutomationIdentifier(
            *action, QStringLiteral("recentFileAction%1").arg(index));
        action->setToolTip(path);
        connect(action, &QAction::triggered, this,
                [this, path](void) { emit OpenPathRequested(path); });
        ++index;
    }
}

} // namespace edit_atlas::app
