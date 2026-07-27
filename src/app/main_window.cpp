#include <edit_atlas/app/main_window.hpp>

#include <edit_atlas/core/version.hpp>

#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QString>
#include <QTranslator>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include <string>

namespace edit_atlas::app {

MainWindow::MainWindow(const core::FormatRegistry &registry,
                       QTranslator &translator,
                       ApplicationLanguage initial_language, QWidget *parent)
    : QMainWindow(parent), registry_(registry), translator_(translator),
      language_(initial_language) {
    setObjectName(QStringLiteral("mainWindow"));
    resize(640, 360);
    setMinimumSize(480, 300);

    BuildMenus();
    BuildUi();
    RetranslateUi();
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUi();
    }
}

void MainWindow::BuildMenus(void) {
    file_menu_ = menuBar()->addMenu(QString{});
    exit_action_ = file_menu_->addAction(QString{});
    exit_action_->setShortcut(QKeySequence::Quit);
    connect(exit_action_, &QAction::triggered, this, &QWidget::close);

    help_menu_ = menuBar()->addMenu(QString{});
    about_action_ = help_menu_->addAction(QString{});
    connect(about_action_, &QAction::triggered, this,
            &MainWindow::ShowAboutDialog);

    language_selector_ = new QComboBox{this};
    language_selector_->setObjectName(QStringLiteral("languageSelector"));
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
                ChangeLanguage(language);
            });
    menuBar()->setCornerWidget(language_selector_, Qt::TopRightCorner);
}

void MainWindow::BuildUi(void) {
    auto *central_widget = new QWidget{this};
    central_widget->setObjectName(QStringLiteral("applicationShell"));
    setCentralWidget(central_widget);

    auto *root_layout = new QVBoxLayout{central_widget};
    root_layout->setContentsMargins(28, 22, 28, 22);
    root_layout->setSpacing(8);

    title_label_ = new QLabel{central_widget};
    title_label_->setObjectName(QStringLiteral("titleLabel"));
    auto title_font = title_label_->font();
    title_font.setPointSize(18);
    title_font.setWeight(QFont::DemiBold);
    title_label_->setFont(title_font);
    root_layout->addWidget(title_label_);

    subtitle_label_ = new QLabel{central_widget};
    subtitle_label_->setObjectName(QStringLiteral("subtitleLabel"));
    subtitle_label_->setWordWrap(true);
    root_layout->addWidget(subtitle_label_);

    root_layout->addStretch(1);

    empty_title_label_ = new QLabel{central_widget};
    empty_title_label_->setAlignment(Qt::AlignCenter);
    auto empty_title_font = empty_title_label_->font();
    empty_title_font.setPointSize(16);
    empty_title_font.setWeight(QFont::DemiBold);
    empty_title_label_->setFont(empty_title_font);
    root_layout->addWidget(empty_title_label_);

    empty_description_label_ = new QLabel{central_widget};
    empty_description_label_->setObjectName(
        QStringLiteral("emptyDescriptionLabel"));
    empty_description_label_->setAlignment(Qt::AlignCenter);
    empty_description_label_->setWordWrap(true);
    root_layout->addWidget(empty_description_label_);

    root_layout->addStretch(1);

    privacy_label_ = new QLabel{central_widget};
    privacy_label_->setObjectName(QStringLiteral("privacyLabel"));
    privacy_label_->setWordWrap(true);
    root_layout->addWidget(privacy_label_);
}

void MainWindow::ShowAboutDialog(void) {
    const auto version = QString::fromStdString(std::string{core::Version()});
    QMessageBox::about(
        this, tr("About Edit Atlas"),
        tr("Edit Atlas %1\n\nInspect editorial timelines and export "
           "structured reports.")
            .arg(version));
}

void MainWindow::ChangeLanguage(ApplicationLanguage language) {
    if (language == language_) {
        return;
    }

    const auto previous_language = language_;
    language_ = language;
    if (!SetApplicationLanguage(translator_, language_)) {
        language_ = previous_language;
        static_cast<void>(
            SetApplicationLanguage(translator_, previous_language));
        RetranslateUi();
        return;
    }

    SaveApplicationLanguage(language_);
    RetranslateUi();
}

void MainWindow::RetranslateUi(void) {
    setWindowTitle(tr("Edit Atlas"));
    file_menu_->setTitle(tr("&File"));
    exit_action_->setText(tr("E&xit"));
    help_menu_->setTitle(tr("&Help"));
    about_action_->setText(tr("&About Edit Atlas"));

    const QSignalBlocker blocker{language_selector_};
    const auto language_value = static_cast<int>(language_);
    language_selector_->setCurrentIndex(
        language_selector_->findData(language_value));
    language_selector_->setToolTip(tr("Language"));
    language_selector_->setAccessibleName(tr("Language"));

    title_label_->setText(tr("Edit Atlas"));
    subtitle_label_->setText(tr("Private tools for editorial timelines."));
    empty_title_label_->setText(tr("No timeline open"));
    empty_description_label_->setText(
        tr("The document-opening workflow is not implemented yet."));
    privacy_label_->setText(
        tr("Your media and timeline data stay on this computer."));
}

} // namespace edit_atlas::app
