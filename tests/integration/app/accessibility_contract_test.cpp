#include <edit_atlas/app/application_menu_bar.hpp>
#include <edit_atlas/app/main_window.hpp>
#include <edit_atlas/app/spreadsheet_export_options_dialog.hpp>
#include <edit_atlas/app/timeline_document_view.hpp>
#include <edit_atlas/app/translation.hpp>

#include "event_projection_dialog.hpp"

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/timeline_filter.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QString>
#include <QTranslator>
#include <QUrl>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <variant>

namespace edit_atlas::app {
namespace {

template <typename Widget>
[[nodiscard]] Widget *FindByAccessibleIdentifier(QWidget &root,
                                                 const QString &identifier) {
    if (auto *self = qobject_cast<Widget *>(&root);
        self != nullptr && self->accessibleIdentifier() == identifier) {
        return self;
    }
    const auto widgets = root.findChildren<Widget *>();
    const auto match =
        std::ranges::find_if(widgets, [&identifier](const Widget *widget) {
            return widget->accessibleIdentifier() == identifier;
        });
    return match == widgets.end() ? nullptr : *match;
}

[[nodiscard]] core::TimelineDocument Document(void) {
    const auto rate = core::FrameRate::Create(24, 1).value();
    const auto start = core::Timecode::FromFrameCount(
                           0, rate, core::TimecodeMode::kNonDropFrame)
                           .value();
    const auto end = core::Timecode::FromFrameCount(
                         24, rate, core::TimecodeMode::kNonDropFrame)
                         .value();
    const auto range = core::TimecodeRange::Create(start, end).value();
    return core::TimelineDocument{
        .title = "Accessibility timeline",
        .frame_rate = rate,
        .timecode_mode = core::TimecodeMode::kNonDropFrame,
        .events =
            {
                core::EditEvent{
                    .identifier = "001",
                    .reel = "AX",
                    .track =
                        core::Track{
                            .kind = core::TrackKind::kVideo,
                            .identifier = "V",
                        },
                    .edit_type = core::EditType::kCut,
                    .transition = std::nullopt,
                    .source_range = range,
                    .record_range = range,
                    .comments = {},
                    .metadata = {},
                    .provenance = std::nullopt,
                },
            },
        .metadata = {},
        .diagnostics = {},
        .provenance = std::nullopt,
    };
}

[[nodiscard]] support::DiagnosticEnvironment DiagnosticEnvironment(void) {
    return support::DiagnosticEnvironment{
        .application_version = "test",
        .operating_system = "test",
        .architecture = "test",
        .qt_version = "test",
        .platform_plugin = "test",
        .importer_formats = {"cmx3600"},
        .exporter_formats = {"xlsx"},
    };
}

void ExpectUniqueIdentifiers(QWidget &root) {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QSet<QString> identifiers;
    auto widgets = root.findChildren<QWidget *>();
    widgets.prepend(&root);
    for (const auto *widget : widgets) {
        const auto identifier = widget->accessibleIdentifier();
        if (identifier.isEmpty()) {
            continue;
        }
        EXPECT_FALSE(identifiers.contains(identifier))
            << identifier.toStdString();
        identifiers.insert(identifier);
    }
    for (const auto *action : root.findChildren<QAction *>()) {
        const auto identifier =
            action->property("accessibleIdentifier").toString();
        if (identifier.isEmpty()) {
            continue;
        }
        EXPECT_FALSE(identifiers.contains(identifier))
            << identifier.toStdString();
        identifiers.insert(identifier);
    }
}

[[nodiscard]] QSet<QString> AccessibleIdentifiers(QWidget &root) {
    QSet<QString> identifiers;
    auto widgets = root.findChildren<QWidget *>();
    widgets.prepend(&root);
    for (const auto *widget : widgets) {
        if (!widget->accessibleIdentifier().isEmpty()) {
            identifiers.insert(widget->accessibleIdentifier());
        }
    }
    for (const auto *action : root.findChildren<QAction *>()) {
        const auto identifier =
            action->property("accessibleIdentifier").toString();
        if (!identifier.isEmpty()) {
            identifiers.insert(identifier);
        }
    }
    return identifiers;
}

[[nodiscard]] bool WaitUntil(const std::function<bool(void)> &condition,
                             int timeout_milliseconds = 5'000) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeout_milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return condition();
}

TEST(AccessibilityContractTest,
     CoversPersistentControlsAndKeepsIdentifiersUnique) {
    QSettings settings;
    settings.clear();
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    QTranslator translator;
    MainWindow window{*registry, translator, ApplicationLanguage::kEnglish,
                      std::filesystem::path{}, DiagnosticEnvironment()};

    constexpr std::array required_identifiers{
        "mainWindow",
        "applicationMenuBar",
        "fileMenu",
        "recentFilesMenu",
        "helpMenu",
        "languageSelector",
        "applicationShell",
        "documentStack",
        "emptyOpenButton",
        "loadingLabel",
        "timelineTitleLabel",
        "timelineSummary",
        "timelineExportButton",
        "timelineFilter",
        "templateSelector",
        "templatePrimaryButton",
        "templateActionsButton",
        "templateActionsMenu",
        "filterCombination",
        "addFilterConditionButton",
        "clearFiltersButton",
        "filterConditionsScrollArea",
        "filterCondition0",
        "filterCondition0Field",
        "filterCondition0Text",
        "filterCondition0TrackKind",
        "filterCondition0EditType",
        "filterCondition0MatchCase",
        "filterCondition0MatchWholeWord",
        "filterCondition0RegularExpression",
        "filterCondition0Remove",
        "filterErrorLabel",
        "filterResultLabel",
        "eventTable",
        "failureDescriptionLabel",
        "failureOpenButton",
        "diagnosticsTree",
    };
    for (const auto *identifier : required_identifiers) {
        EXPECT_NE(FindByAccessibleIdentifier<QWidget>(
                      window, QString::fromLatin1(identifier)),
                  nullptr)
            << identifier;
    }

    constexpr std::array action_identifiers{
        "openDocumentAction",
        "rememberRecentFilesAction",
        "exportAction",
        "exitAction",
        "exportDiagnosticLogsAction",
        "aboutAction",
        "saveTemplateAction",
        "editExportColumnsAction",
        "renameTemplateAction",
        "duplicateTemplateAction",
        "deleteTemplateAction",
    };
    for (const auto *identifier : action_identifiers) {
        auto *action =
            window.findChild<QAction *>(QString::fromLatin1(identifier));
        ASSERT_NE(action, nullptr) << identifier;
        EXPECT_EQ(action->property("accessibleIdentifier").toString(),
                  QString::fromLatin1(identifier));
    }

    auto *menu = window.findChild<ApplicationMenuBar *>();
    auto *remember = window.findChild<QAction *>(
        QStringLiteral("rememberRecentFilesAction"));
    ASSERT_NE(menu, nullptr);
    ASSERT_NE(remember, nullptr);
    remember->setChecked(true);
    menu->RememberRecentFile(QStringLiteral("/isolated/recent.edl"));
    auto *recent =
        window.findChild<QAction *>(QStringLiteral("recentFileAction0"));
    ASSERT_NE(recent, nullptr);
    EXPECT_EQ(recent->property("accessibleIdentifier").toString(),
              QStringLiteral("recentFileAction0"));
    ExpectUniqueIdentifiers(window);
}

TEST(AccessibilityContractTest, CoversSpreadsheetOptionsAndProjectionControls) {
    const auto projection = core::DefaultTimelineEventProjection();
    SpreadsheetExportOptionsDialog dialog{ApplicationLanguage::kEnglish,
                                          projection};
    constexpr std::array required_identifiers{
        "spreadsheetOptionsDialog",
        "workbookLanguageSelector",
        "includeTimelineSheetCheckBox",
        "includeDiagnosticsSheetCheckBox",
        "eventProjectionWidget",
        "eventColumnsList",
        "moveColumnUpButton",
        "moveColumnDownButton",
        "columnSelectionErrorLabel",
        "continueSpreadsheetExportButton",
        "cancelSpreadsheetExportButton",
    };
    for (const auto *identifier : required_identifiers) {
        EXPECT_NE(FindByAccessibleIdentifier<QWidget>(
                      dialog, QString::fromLatin1(identifier)),
                  nullptr)
            << identifier;
    }
    ExpectUniqueIdentifiers(dialog);

    auto *columns = FindByAccessibleIdentifier<QListWidget>(
        dialog, QStringLiteral("eventColumnsList"));
    auto *move_down = FindByAccessibleIdentifier<QPushButton>(
        dialog, QStringLiteral("moveColumnDownButton"));
    auto *timeline = FindByAccessibleIdentifier<QCheckBox>(
        dialog, QStringLiteral("includeTimelineSheetCheckBox"));
    ASSERT_NE(columns, nullptr);
    ASSERT_NE(move_down, nullptr);
    ASSERT_NE(timeline, nullptr);
    const auto original_first = dialog.EventProjection().front();
    columns->setCurrentRow(0);
    move_down->click();
    timeline->setChecked(false);

    const auto changed = dialog.EventProjection();
    ASSERT_GE(changed.size(), 2);
    EXPECT_NE(changed.front(), original_first);
    const auto options = dialog.Options();
    const auto timeline_option = std::ranges::find(
        options, std::string{formats::xlsx::kIncludeTimelineSheetOption},
        &core::MetadataEntry::key);
    ASSERT_NE(timeline_option, options.end());
    EXPECT_EQ(std::get<bool>(timeline_option->value), false);
}

TEST(AccessibilityContractTest, CoversTemplateProjectionDialogControls) {
    const auto projection = core::DefaultTimelineEventProjection();
    EventProjectionDialog dialog{projection};
    constexpr std::array required_identifiers{
        "eventProjectionDialog",  "eventProjectionWidget",
        "eventColumnsList",       "moveColumnUpButton",
        "moveColumnDownButton",   "saveProjectionButton",
        "cancelProjectionButton",
    };
    for (const auto *identifier : required_identifiers) {
        EXPECT_NE(FindByAccessibleIdentifier<QWidget>(
                      dialog, QString::fromLatin1(identifier)),
                  nullptr)
            << identifier;
    }
    ExpectUniqueIdentifiers(dialog);
}

TEST(AccessibilityContractTest,
     KeepsDynamicFilterRowsUniqueAndUsesTypedEditors) {
    TimelineDocumentView view;
    const auto document = Document();
    view.ShowTimeline(document, QStringLiteral("timeline.edl"), {});
    view.resize(700, 360);
    view.show();
    QSignalSpy changed{&view, &TimelineDocumentView::FilterChanged};
    view.SetFilterQuery(services::TimelineFilterQuery{
        .combination = services::TimelineFilterCombination::kAny,
        .conditions =
            {
                services::TimelineTextFilterCondition{
                    .field = services::TimelineTextFilterField::kComments,
                    .text = "dialogue",
                    .match_case = true,
                    .match_whole_word = true,
                    .regular_expression = false,
                },
                services::TimelineTrackKindFilterCondition{
                    .track_kind = core::TrackKind::kAudio,
                },
                services::TimelineDurationFilterCondition{
                    .frames = 24,
                },
            },
    });
    EXPECT_GE(changed.count(), 1);
    EXPECT_EQ(view.FilterQuery().conditions.size(), 3);
    EXPECT_NE(FindByAccessibleIdentifier<QComboBox>(
                  view, QStringLiteral("filterCondition1TrackKind")),
              nullptr);
    EXPECT_NE(FindByAccessibleIdentifier<QLineEdit>(
                  view, QStringLiteral("filterCondition2Text")),
              nullptr);
    ExpectUniqueIdentifiers(view);

    auto *add = FindByAccessibleIdentifier<QPushButton>(
        view, QStringLiteral("addFilterConditionButton"));
    ASSERT_NE(add, nullptr);
    for (int index = 0; index < 10; ++index) {
        add->click();
    }
    auto *scroll = FindByAccessibleIdentifier<QScrollArea>(
        view, QStringLiteral("filterConditionsScrollArea"));
    ASSERT_NE(scroll, nullptr);
    EXPECT_TRUE(WaitUntil(
        [scroll](void) { return scroll->verticalScrollBar()->maximum() > 0; }));
    ExpectUniqueIdentifiers(view);

    view.SetFilterQuery(services::TimelineFilterQuery{
        .combination = services::TimelineFilterCombination::kAll,
        .conditions =
            {
                services::TimelineTextFilterCondition{
                    .field = services::TimelineTextFilterField::kComments,
                    .text = "(",
                    .match_case = false,
                    .match_whole_word = false,
                    .regular_expression = true,
                },
            },
    });
    const auto filter_result =
        services::FilterTimelineEvents(document, view.FilterQuery());
    ASSERT_FALSE(filter_result.has_value());
    view.SetFilterError(QStringLiteral("Invalid regular expression"));
    auto *error = FindByAccessibleIdentifier<QWidget>(
        view, QStringLiteral("filterErrorLabel"));
    ASSERT_NE(error, nullptr);
    EXPECT_TRUE(error->isVisibleTo(&view));

    auto *clear = FindByAccessibleIdentifier<QPushButton>(
        view, QStringLiteral("clearFiltersButton"));
    ASSERT_NE(clear, nullptr);
    clear->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(view.FilterQuery().conditions.size(), 1);
    EXPECT_EQ(FindByAccessibleIdentifier<QWidget>(
                  view, QStringLiteral("filterCondition1")),
              nullptr);
}

TEST(AccessibilityContractTest, PersistsLanguageWithoutChangingIdentifiers) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    QTranslator translator;
    ASSERT_TRUE(SetApplicationLanguage(
        translator, ApplicationLanguage::kBrazilianPortuguese));
    MainWindow window{*registry, translator,
                      ApplicationLanguage::kBrazilianPortuguese,
                      std::filesystem::path{}, DiagnosticEnvironment()};
    auto *selector = FindByAccessibleIdentifier<QComboBox>(
        window, QStringLiteral("languageSelector"));
    auto *open =
        window.findChild<QAction *>(QStringLiteral("openDocumentAction"));
    ASSERT_NE(selector, nullptr);
    ASSERT_NE(open, nullptr);
    EXPECT_EQ(open->text(), QStringLiteral("&Abrir linha do tempo…"));
    const auto identifiers = AccessibleIdentifiers(window);
    selector->setCurrentIndex(
        selector->findData(static_cast<int>(ApplicationLanguage::kEnglish)));

    EXPECT_EQ(ConfiguredApplicationLanguage(), ApplicationLanguage::kEnglish);
    EXPECT_EQ(selector->accessibleIdentifier(),
              QStringLiteral("languageSelector"));
    EXPECT_EQ(AccessibleIdentifiers(window), identifiers);
    EXPECT_EQ(open->text(), QStringLiteral("&Open Timeline…"));
    EXPECT_EQ(window.windowTitle(), QStringLiteral("Edit Atlas"));
}

TEST(AccessibilityContractTest, AcceptsOnlyLocalFilesForDragAndDrop) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    QTranslator translator;
    MainWindow window{*registry, translator, ApplicationLanguage::kEnglish,
                      std::filesystem::path{}, DiagnosticEnvironment()};

    QMimeData remote_data;
    remote_data.setUrls(
        {QUrl{QStringLiteral("https://example.invalid/a.edl")}});
    QDragEnterEvent remote_event{QPoint{10, 10}, Qt::CopyAction, &remote_data,
                                 Qt::LeftButton, Qt::NoModifier};
    QCoreApplication::sendEvent(&window, &remote_event);
    EXPECT_FALSE(remote_event.isAccepted());

    QMimeData local_data;
    local_data.setUrls(
        {QUrl::fromLocalFile(QStringLiteral("/missing/timeline.edl"))});
    QDragEnterEvent local_event{QPoint{10, 10}, Qt::CopyAction, &local_data,
                                Qt::LeftButton, Qt::NoModifier};
    QCoreApplication::sendEvent(&window, &local_event);
    EXPECT_TRUE(local_event.isAccepted());

    window.show();
    QDropEvent drop_event{QPointF{10.0, 10.0}, Qt::CopyAction, &local_data,
                          Qt::LeftButton, Qt::NoModifier};
    QCoreApplication::sendEvent(&window, &drop_event);
    EXPECT_TRUE(drop_event.isAccepted());
    auto *failure = FindByAccessibleIdentifier<QPushButton>(
        window, QStringLiteral("failureOpenButton"));
    ASSERT_NE(failure, nullptr);
    EXPECT_TRUE(WaitUntil(
        [&window, failure](void) { return failure->isVisibleTo(&window); }));
}

} // namespace
} // namespace edit_atlas::app
