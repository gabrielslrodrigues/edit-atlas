#include <edit_atlas/frontends/widgets/application_menu_bar.hpp>
#include <edit_atlas/frontends/widgets/main_window.hpp>
#include <edit_atlas/frontends/widgets/spreadsheet_export_options_dialog.hpp>
#include <edit_atlas/frontends/widgets/timeline_document_view.hpp>
#include <edit_atlas/presentation/timeline_event_model.hpp>
#include <edit_atlas/presentation/translation.hpp>

#include "event_projection_dialog.hpp"

#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/timecode.hpp>
#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/formats/xlsx/xlsx_exporter.hpp>

#include <edit_atlas/services/built_in_formats.hpp>
#include <edit_atlas/services/timeline_filter.hpp>

#include <edit_atlas/support/support_bundle.hpp>

#include <QAccessible>
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

namespace edit_atlas::frontends::widgets {
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
    MainWindow window{*registry, translator,
                      presentation::ApplicationLanguage::kEnglish,
                      std::filesystem::path{}, DiagnosticEnvironment()};

    constexpr std::array required_identifiers{
        "mainWindow",
        "applicationMenuBar",
        "fileMenuPopup",
        "recentFilesMenuPopup",
        "helpMenuPopup",
        "languageMenuPopup",
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
        "fileMenu",
        "recentFilesMenu",
        "helpMenu",
        "languageSelector",
        "brazilianPortugueseLanguageAction",
        "englishLanguageAction",
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
    SpreadsheetExportOptionsDialog dialog{
        presentation::ApplicationLanguage::kEnglish, projection};
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
        "renderedVideoGroup",
        "renderedVideoPathField",
        "browseRenderedVideoButton",
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
    auto initial_frame_row = -1;
    for (int row = 0; row < columns->count(); ++row) {
        if (static_cast<core::TimelineEventField>(
                columns->item(row)->data(Qt::UserRole).toInt()) ==
            core::TimelineEventField::kInitialFrame) {
            initial_frame_row = row;
            break;
        }
    }
    ASSERT_GE(initial_frame_row, 0);
    EXPECT_EQ(columns->item(initial_frame_row)->checkState(), Qt::Unchecked);
    columns->item(initial_frame_row)->setCheckState(Qt::Checked);
    auto *video_group = FindByAccessibleIdentifier<QWidget>(
        dialog, QStringLiteral("renderedVideoGroup"));
    auto *video_path = FindByAccessibleIdentifier<QLineEdit>(
        dialog, QStringLiteral("renderedVideoPathField"));
    auto *continue_button = FindByAccessibleIdentifier<QPushButton>(
        dialog, QStringLiteral("continueSpreadsheetExportButton"));
    ASSERT_NE(video_group, nullptr);
    ASSERT_NE(video_path, nullptr);
    ASSERT_NE(continue_button, nullptr);
    EXPECT_FALSE(video_group->isHidden());
    EXPECT_FALSE(continue_button->isEnabled());
    video_path->setText(QStringLiteral("matching.mov"));
    EXPECT_TRUE(continue_button->isEnabled());
    const auto original_first = dialog.EventProjection().front();
    columns->setCurrentRow(0);
    move_down->click();
    timeline->setChecked(false);

    const auto changed = dialog.EventProjection();
    ASSERT_GE(changed.size(), 2);
    EXPECT_NE(changed.front(), original_first);
    EXPECT_NE(
        std::ranges::find(changed, core::TimelineEventField::kInitialFrame),
        changed.end());
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

#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
TEST(AccessibilityContractTest,
     ProjectionChildToggleActionChangesCheckedState) {
    const auto projection = core::DefaultTimelineEventProjection();
    EventProjectionDialog dialog{projection};
    auto *columns = FindByAccessibleIdentifier<QListWidget>(
        dialog, QStringLiteral("eventColumnsList"));
    ASSERT_NE(columns, nullptr);
    auto *list_interface = QAccessible::queryAccessibleInterface(columns);
    ASSERT_NE(list_interface, nullptr);
    auto *child_interface = list_interface->child(0);
    ASSERT_NE(child_interface, nullptr);
    auto *action_interface = child_interface->actionInterface();
    ASSERT_NE(action_interface, nullptr);
    auto *item = columns->item(0);
    ASSERT_NE(item, nullptr);
    ASSERT_EQ(item->checkState(), Qt::Checked);

    action_interface->doAction(QAccessibleActionInterface::toggleAction());

    EXPECT_EQ(item->checkState(), Qt::Unchecked);
}

TEST(AccessibilityContractTest,
     ReleasesProjectionChildInterfacesWhenDialogCloses) {
    QAccessible::Id child_identifier = 0;
    {
        const auto projection = core::DefaultTimelineEventProjection();
        EventProjectionDialog dialog{projection};
        auto *columns = FindByAccessibleIdentifier<QListWidget>(
            dialog, QStringLiteral("eventColumnsList"));
        ASSERT_NE(columns, nullptr);
        auto *list_interface = QAccessible::queryAccessibleInterface(columns);
        ASSERT_NE(list_interface, nullptr);
        auto *child_interface = list_interface->child(0);
        ASSERT_NE(child_interface, nullptr);
        child_identifier = QAccessible::uniqueId(child_interface);
        ASSERT_NE(child_identifier, 0U);
        EXPECT_EQ(QAccessible::accessibleInterface(child_identifier),
                  child_interface);
    }

    EXPECT_EQ(QAccessible::accessibleInterface(child_identifier), nullptr);
}
#endif

TEST(AccessibilityContractTest,
     KeepsDynamicFilterRowsUniqueAndUsesTypedEditors) {
    TimelineDocumentView view;
    const auto document = Document();
    presentation::TimelineEventModel event_model;
    event_model.SetDocument(&document);
    view.SetEventModel(event_model);
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
    ASSERT_TRUE(presentation::SetApplicationLanguage(
        translator, presentation::ApplicationLanguage::kBrazilianPortuguese));
    MainWindow window{*registry, translator,
                      presentation::ApplicationLanguage::kBrazilianPortuguese,
                      std::filesystem::path{}, DiagnosticEnvironment()};
    auto *selector =
        window.findChild<QAction *>(QStringLiteral("languageSelector"));
    auto *english =
        window.findChild<QAction *>(QStringLiteral("englishLanguageAction"));
    auto *open =
        window.findChild<QAction *>(QStringLiteral("openDocumentAction"));
    ASSERT_NE(selector, nullptr);
    ASSERT_NE(english, nullptr);
    ASSERT_NE(open, nullptr);
    EXPECT_EQ(open->text(), QStringLiteral("&Abrir linha do tempo…"));
    const auto identifiers = AccessibleIdentifiers(window);
    english->trigger();

    EXPECT_EQ(presentation::ConfiguredApplicationLanguage(),
              presentation::ApplicationLanguage::kEnglish);
    EXPECT_EQ(selector->property("accessibleIdentifier").toString(),
              QStringLiteral("languageSelector"));
    EXPECT_TRUE(english->isChecked());
    EXPECT_EQ(AccessibleIdentifiers(window), identifiers);
    EXPECT_EQ(open->text(), QStringLiteral("&Open Timeline…"));
    EXPECT_EQ(window.windowTitle(), QStringLiteral("Edit Atlas"));
}

TEST(AccessibilityContractTest, AcceptsOnlyLocalFilesForDragAndDrop) {
    auto registry = services::CreateBuiltInFormatRegistry();
    ASSERT_TRUE(registry.has_value());
    QTranslator translator;
    MainWindow window{*registry, translator,
                      presentation::ApplicationLanguage::kEnglish,
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
} // namespace edit_atlas::frontends::widgets
