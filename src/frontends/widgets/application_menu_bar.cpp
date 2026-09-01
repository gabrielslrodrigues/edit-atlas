#include <edit_atlas/frontends/widgets/application_menu_bar.hpp>

#include "accessibility.hpp"

#include <edit_atlas/presentation/application_state.hpp>

#include <QAction>
#include <QActionGroup>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QString>
#include <QWidget>

#include <cstddef>
#include <filesystem>
#include <string>

namespace edit_atlas::frontends::widgets {
namespace {

[[nodiscard]] std::filesystem::path FilesystemPath(const QString &path) {
    const auto utf8 = path.toUtf8();
    return std::filesystem::path{
        std::u8string{reinterpret_cast<const char8_t *>(utf8.constData()),
                      static_cast<std::size_t>(utf8.size())}};
}

[[nodiscard]] QString PathText(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return QString::fromUtf8(reinterpret_cast<const char *>(utf8.data()),
                             static_cast<qsizetype>(utf8.size()));
}

} // namespace

ApplicationMenuBar::ApplicationMenuBar(
    presentation::ApplicationLanguage initial_language, QWidget *parent)
    : QMenuBar{parent} {
    BuildUi(initial_language);
    RetranslateUi();
}

void ApplicationMenuBar::RememberRecentFile(const QString &path) {
    presentation::RecordRecentFile(FilesystemPath(path));
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
    language_menu_->setTitle(tr("&Language"));
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

void ApplicationMenuBar::SetLanguage(
    presentation::ApplicationLanguage language) {
    brazilian_portuguese_action_->setChecked(
        language == presentation::ApplicationLanguage::kBrazilianPortuguese);
    english_action_->setChecked(language ==
                                presentation::ApplicationLanguage::kEnglish);
}

void ApplicationMenuBar::BuildUi(
    presentation::ApplicationLanguage initial_language) {
    file_menu_ = addMenu(QString{});

    open_action_ = file_menu_->addAction(QString{});
    open_action_->setShortcut(QKeySequence::Open);
    connect(open_action_, &QAction::triggered, this,
            &ApplicationMenuBar::openRequested);

    recent_files_menu_ = file_menu_->addMenu(QString{});

    remember_recent_action_ = file_menu_->addAction(QString{});
    remember_recent_action_->setCheckable(true);
    remember_recent_action_->setChecked(
        presentation::RememberRecentFilesEnabled());
    connect(remember_recent_action_, &QAction::toggled, this,
            &ApplicationMenuBar::SetRememberRecentFiles);

    file_menu_->addSeparator();
    export_action_ = file_menu_->addAction(QString{});
    connect(export_action_, &QAction::triggered, this,
            &ApplicationMenuBar::exportSpreadsheetRequested);

    file_menu_->addSeparator();
    exit_action_ = file_menu_->addAction(QString{});
    exit_action_->setShortcut(QKeySequence::Quit);
    connect(exit_action_, &QAction::triggered, this,
            &ApplicationMenuBar::exitRequested);

    help_menu_ = addMenu(QString{});
    export_logs_action_ = help_menu_->addAction(QString{});
    connect(export_logs_action_, &QAction::triggered, this,
            &ApplicationMenuBar::exportDiagnosticLogsRequested);
    help_menu_->addSeparator();
    about_action_ = help_menu_->addAction(QString{});
    connect(about_action_, &QAction::triggered, this,
            &ApplicationMenuBar::aboutRequested);

    language_menu_ = addMenu(QString{});
    brazilian_portuguese_action_ =
        language_menu_->addAction(QStringLiteral("Português (Brasil)"));
    english_action_ = language_menu_->addAction(QStringLiteral("English"));
    auto *language_group = new QActionGroup{this};
    language_group->setExclusive(true);
    brazilian_portuguese_action_->setCheckable(true);
    english_action_->setCheckable(true);
    language_group->addAction(brazilian_portuguese_action_);
    language_group->addAction(english_action_);
    brazilian_portuguese_action_->setData(static_cast<int>(
        presentation::ApplicationLanguage::kBrazilianPortuguese));
    english_action_->setData(
        static_cast<int>(presentation::ApplicationLanguage::kEnglish));
    connect(language_group, &QActionGroup::triggered, this,
            [this](const QAction *action) {
                const auto language =
                    static_cast<presentation::ApplicationLanguage>(
                        action->data().toInt());
                emit languageSelected(language);
            });

    SetAutomationIdentifier(*this, u"applicationMenuBar");
    SetAutomationIdentifier(*file_menu_, u"fileMenuPopup");
    SetAutomationIdentifier(*file_menu_->menuAction(), u"fileMenu");
    SetAutomationIdentifier(*open_action_, u"openDocumentAction");
    SetAutomationIdentifier(*recent_files_menu_, u"recentFilesMenuPopup");
    SetAutomationIdentifier(*recent_files_menu_->menuAction(),
                            u"recentFilesMenu");
    SetAutomationIdentifier(*remember_recent_action_,
                            u"rememberRecentFilesAction");
    SetAutomationIdentifier(*export_action_, u"exportAction");
    SetAutomationIdentifier(*exit_action_, u"exitAction");
    SetAutomationIdentifier(*help_menu_, u"helpMenuPopup");
    SetAutomationIdentifier(*help_menu_->menuAction(), u"helpMenu");
    SetAutomationIdentifier(*export_logs_action_,
                            u"exportDiagnosticLogsAction");
    SetAutomationIdentifier(*about_action_, u"aboutAction");
    SetAutomationIdentifier(*language_menu_, u"languageMenuPopup");
    SetAutomationIdentifier(*language_menu_->menuAction(), u"languageSelector");
    SetAutomationIdentifier(*brazilian_portuguese_action_,
                            u"brazilianPortugueseLanguageAction");
    SetAutomationIdentifier(*english_action_, u"englishLanguageAction");

    SetLanguage(initial_language);
    UpdateActions();
}

void ApplicationMenuBar::SetRememberRecentFiles(bool enabled) {
    presentation::SetRememberRecentFilesEnabled(enabled);
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

    const auto recent_files = presentation::ConfiguredRecentFiles();
    if (recent_files.empty()) {
        auto *empty = recent_files_menu_->addAction(tr("No recent files"));
        empty->setEnabled(false);
        return;
    }

    qsizetype index = 0;
    for (const auto &path : recent_files) {
        const auto path_text = PathText(path);
        auto *action =
            recent_files_menu_->addAction(QFileInfo{path_text}.fileName());
        SetAutomationIdentifier(
            *action, QStringLiteral("recentFileAction%1").arg(index));
        action->setToolTip(path_text);
        connect(action, &QAction::triggered, this,
                [this, path_text](void) { emit openPathRequested(path_text); });
        ++index;
    }
}

} // namespace edit_atlas::frontends::widgets
