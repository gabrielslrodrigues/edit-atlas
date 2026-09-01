#include "timeline_template_controller.hpp"

#include "accessibility.hpp"
#include "event_projection_dialog.hpp"

#include <edit_atlas/frontends/widgets/timeline_document_view.hpp>

#include <edit_atlas/presentation/application_state.hpp>
#include <edit_atlas/presentation/timeline_template_view_model.hpp>

#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_template.hpp>
#include <edit_atlas/services/timeline_template_service.hpp>

#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <spdlog/spdlog.h>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace edit_atlas::frontends::widgets {
namespace {

[[nodiscard]] QString Utf8(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] std::string Utf8String(const QString &text) {
    const auto utf8 = text.toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

[[nodiscard]] QString PathText(const std::filesystem::path &path) {
    const auto utf8 = path.generic_u8string();
    return QString::fromUtf8(reinterpret_cast<const char *>(utf8.data()),
                             static_cast<qsizetype>(utf8.size()));
}

void ShowWarning(QWidget &window, const QString &title, const QString &text,
                 const QString &identifier) {
    QMessageBox message{QMessageBox::Warning, title, text,
                        QMessageBox::NoButton, &window};
    auto *close = message.addButton(TimelineTemplateController::tr("Close"),
                                    QMessageBox::RejectRole);

    SetAutomationIdentifier(message, identifier);
    SetAutomationIdentifier(*close, u"closeDialogButton");
    message.exec();
}

} // namespace

TimelineTemplateController::TimelineTemplateController(
    TimelineDocumentView &view, QWidget &window, QObject *parent)
    : QObject{parent}, view_{view}, window_{window},
      view_model_{presentation::ConfiguredTemplateDirectory()} {
    connect(&view_, &TimelineDocumentView::templateSelected, this,
            &TimelineTemplateController::ApplyTemplate);
    connect(&view_, &TimelineDocumentView::saveTemplateRequested, this,
            &TimelineTemplateController::SaveTemplate);
    connect(&view_, &TimelineDocumentView::updateTemplateRequested, this,
            &TimelineTemplateController::UpdateTemplate);
    connect(&view_, &TimelineDocumentView::renameTemplateRequested, this,
            &TimelineTemplateController::RenameTemplate);
    connect(&view_, &TimelineDocumentView::duplicateTemplateRequested, this,
            &TimelineTemplateController::DuplicateTemplate);
    connect(&view_, &TimelineDocumentView::deleteTemplateRequested, this,
            &TimelineTemplateController::DeleteTemplate);
    connect(&view_, &TimelineDocumentView::editColumnsRequested, this,
            &TimelineTemplateController::EditExportColumns);
    LoadTemplates();
}

void TimelineTemplateController::ApplyTemplate(const QString &identifier) {
    if (identifier.isEmpty()) {
        view_model_.SelectNoTemplate();
    } else {
        static_cast<void>(view_model_.SelectTemplate(Utf8String(identifier)));
    }
    view_.SetFilterQuery(view_model_.FilterQuery());
    RefreshTemplateState();
}

void TimelineTemplateController::DeleteTemplate(void) {
    const auto *value = view_model_.ActiveTemplate();
    if (value == nullptr) {
        return;
    }
    QMessageBox confirmation{
        QMessageBox::Question,
        tr("Delete Template?"),
        tr("Delete the template “%1”? This cannot be undone.")
            .arg(Utf8(value->name)),
        QMessageBox::NoButton,
        &window_,
    };
    auto *delete_button = confirmation.addButton(QMessageBox::Yes);
    auto *cancel_button = confirmation.addButton(QMessageBox::No);

    SetAutomationIdentifier(confirmation, u"deleteTemplateDialog");
    SetAutomationIdentifier(*delete_button, u"confirmDeleteTemplateButton");
    SetAutomationIdentifier(*cancel_button, u"cancelDeleteTemplateButton");

    confirmation.setDefaultButton(cancel_button);
    confirmation.exec();
    if (confirmation.clickedButton() != delete_button) {
        return;
    }
    const auto result = view_model_.RemoveActive();
    if (!result.has_value()) {
        ShowCommandFailure(tr("Could Not Delete Template"), result.error());
        return;
    }
    view_.SetFilterQuery(view_model_.FilterQuery());
    RefreshTemplateState();
}

void TimelineTemplateController::DuplicateTemplate(void) {
    const auto *value = view_model_.ActiveTemplate();
    if (value == nullptr) {
        return;
    }
    const auto name = PromptForTemplateName(
        tr("Duplicate Template"), tr("Name for the copy:"),
        tr("%1 copy").arg(Utf8(value->name)));
    if (!name.has_value()) {
        return;
    }
    const auto result = view_model_.DuplicateActive(*name);
    if (!result.has_value()) {
        ShowCommandFailure(tr("Could Not Duplicate Template"), result.error());
        return;
    }
    RefreshTemplateState();
}

void TimelineTemplateController::EditExportColumns(void) {
    EventProjectionDialog dialog{view_model_.EventProjection(), &window_};
    if (dialog.exec() == QDialog::Accepted) {
        SetEventProjection(dialog.Projection());
    }
}

std::span<const core::TimelineEventField>
TimelineTemplateController::EventProjection(void) const noexcept {
    return view_model_.EventProjection();
}

void TimelineTemplateController::LoadTemplates(void) {
    const auto result = view_model_.Load();
    if (!result.has_value()) {
        ShowServiceFailure(tr("Could Not Load Templates"), result.error());
        RefreshTemplateState();
        return;
    }
    RefreshTemplateState();
    if (result->empty()) {
        return;
    }

    QStringList details;
    for (const auto &diagnostic : *result) {
        details.push_back(QStringLiteral("%1: %2").arg(
            PathText(diagnostic.path), Utf8(diagnostic.message)));
        SPDLOG_WARN("Skipped timeline template {}: {}",
                    diagnostic.path.string(), diagnostic.message);
    }
    QMessageBox message{
        QMessageBox::Warning,
        tr("Some Templates Could Not Be Loaded"),
        tr("%1 template file(s) were skipped because they are invalid or use "
           "an unsupported version.")
            .arg(static_cast<qulonglong>(result->size())),
        QMessageBox::NoButton,
        &window_,
    };
    message.setDetailedText(details.join(u'\n'));
    auto *close = message.addButton(tr("Close"), QMessageBox::RejectRole);

    SetAutomationIdentifier(message, u"invalidTemplatesDialog");
    SetAutomationIdentifier(*close, u"closeDialogButton");
    message.exec();
}

std::optional<std::string> TimelineTemplateController::PromptForTemplateName(
    const QString &title, const QString &label, const QString &initial) {
    QInputDialog dialog{&window_};
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setTextValue(initial);
    dialog.setTextEchoMode(QLineEdit::Normal);
    SetAutomationIdentifier(dialog, u"templateNameDialog");
    if (auto *editor = dialog.findChild<QLineEdit *>(); editor != nullptr) {
        SetAutomationIdentifier(*editor, u"templateNameEditor");
    }
    SetInputDialogButtonAutomationIdentifiers(
        dialog, u"acceptTemplateNameButton", u"cancelTemplateNameButton");
    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    auto proposed = dialog.textValue().trimmed();
    if (proposed.isEmpty()) {
        ShowWarning(window_, tr("Invalid Template Name"),
                    tr("Enter a name for the template."),
                    QStringLiteral("invalidTemplateNameDialog"));
        return std::nullopt;
    }
    return Utf8String(proposed);
}

void TimelineTemplateController::RefreshTemplateState(void) {
    std::optional<std::string_view> active_identifier;
    if (view_model_.ActiveIdentifier().has_value()) {
        active_identifier = *view_model_.ActiveIdentifier();
    }
    view_.SetTemplates(view_model_.Templates(), active_identifier,
                       view_model_.IsModified());
}

void TimelineTemplateController::RenameTemplate(void) {
    const auto *value = view_model_.ActiveTemplate();
    if (value == nullptr) {
        return;
    }
    const auto previous_name = value->name;
    const auto name = PromptForTemplateName(
        tr("Rename Template"), tr("Template name:"), Utf8(previous_name));
    if (!name.has_value() || *name == previous_name) {
        return;
    }
    const auto result = view_model_.RenameActive(*name);
    if (!result.has_value()) {
        ShowCommandFailure(tr("Could Not Rename Template"), result.error());
        return;
    }
    RefreshTemplateState();
}

void TimelineTemplateController::RestoreForTimeline(void) {
    view_model_.RestoreForTimeline();
    view_.SetFilterQuery(view_model_.FilterQuery());
    RefreshTemplateState();
}

void TimelineTemplateController::SaveTemplate(void) {
    if (!view_model_.IsFilterValid()) {
        ShowInvalidFilter(
            tr("Could Not Save Template"),
            tr("Fix the invalid filter condition before saving a template."));
        return;
    }
    const auto name = PromptForTemplateName(
        tr("Save Filter and Export Template"), tr("Template name:"), {});
    if (!name.has_value()) {
        return;
    }
    const auto result = view_model_.Create(*name);
    if (!result.has_value()) {
        ShowCommandFailure(tr("Could Not Save Template"), result.error());
        return;
    }
    RefreshTemplateState();
}

void TimelineTemplateController::SetEventProjection(
    std::vector<core::TimelineEventField> event_projection) {
    const auto result =
        view_model_.SetEventProjection(std::move(event_projection));
    if (!result.has_value()) {
        ShowCommandFailure(tr("Could Not Change Export Columns"),
                           result.error());
        return;
    }
    RefreshTemplateState();
}

void TimelineTemplateController::SetFilterState(
    services::TimelineFilterQuery filter, bool valid) {
    view_model_.SetFilterState(std::move(filter), valid);
    RefreshTemplateState();
}

void TimelineTemplateController::ShowCommandFailure(
    const QString &title,
    const presentation::TimelineTemplateCommandFailure &failure) {
    if (const auto *service_failure =
            std::get_if<services::TimelineTemplateFailure>(&failure);
        service_failure != nullptr) {
        ShowServiceFailure(title, *service_failure);
        return;
    }

    switch (std::get<presentation::TimelineTemplateCommandError>(failure)) {
    case presentation::TimelineTemplateCommandError::kInvalidFilter:
        SPDLOG_ERROR("Timeline template command was rejected because the "
                     "filter is invalid");
        return;
    case presentation::TimelineTemplateCommandError::kInvalidProjection:
        SPDLOG_ERROR("Timeline template command was rejected because the "
                     "event projection is invalid");
        return;
    case presentation::TimelineTemplateCommandError::kNoActiveTemplate:
        return;
    }
}

void TimelineTemplateController::ShowInvalidFilter(const QString &title,
                                                   const QString &description) {
    ShowWarning(window_, title, description,
                QStringLiteral("invalidTemplateFilterDialog"));
}

void TimelineTemplateController::ShowServiceFailure(
    const QString &title, const services::TimelineTemplateFailure &failure) {
    SPDLOG_ERROR("Timeline template operation failed for {}: {} ({})",
                 failure.path.string(), failure.message,
                 failure.filesystem_error.message());
    const auto description =
        failure.kind == services::TimelineTemplateFailureKind::kNameConflict
            ? tr("A template with that name already exists.")
            : tr("The template could not be stored on this computer.");
    QMessageBox message{QMessageBox::Critical, title, description,
                        QMessageBox::NoButton, &window_};
    auto details = tr("Detail: %1").arg(Utf8(failure.message));
    if (!failure.path.empty()) {
        details.prepend(tr("Path: %1\n").arg(PathText(failure.path)));
    }
    if (failure.filesystem_error) {
        details += tr("\nSystem error: %1")
                       .arg(Utf8(failure.filesystem_error.message()));
    }
    message.setDetailedText(details);
    auto *close = message.addButton(tr("Close"), QMessageBox::RejectRole);

    SetAutomationIdentifier(message, u"templateFailureDialog");
    SetAutomationIdentifier(*close, u"closeDialogButton");
    message.exec();
}

void TimelineTemplateController::UpdateTemplate(void) {
    if (!view_model_.IsFilterValid()) {
        ShowInvalidFilter(
            tr("Could Not Update Template"),
            tr("Fix the invalid filter condition before updating the "
               "template."));
        return;
    }
    const auto result = view_model_.UpdateActive();
    if (!result.has_value()) {
        ShowCommandFailure(tr("Could Not Update Template"), result.error());
        return;
    }
    RefreshTemplateState();
}

} // namespace edit_atlas::frontends::widgets
