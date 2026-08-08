#include "timeline_template_controller.hpp"

#include "accessibility.hpp"

#include <edit_atlas/app/application_state.hpp>
#include <edit_atlas/app/timeline_document_view.hpp>

#include "event_projection_dialog.hpp"

#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_template_service.hpp>

#include <QDialog>
#include <QDialogButtonBox>
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
#include <utility>
#include <vector>

namespace edit_atlas::app {
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

[[nodiscard]] std::vector<core::TimelineEventField>
DefaultEventProjection(void) {
    const auto fields = core::DefaultTimelineEventProjection();
    return {fields.begin(), fields.end()};
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
      service_{ConfiguredTemplateDirectory()},
      event_projection_{DefaultEventProjection()} {
    connect(&view_, &TimelineDocumentView::TemplateSelected, this,
            &TimelineTemplateController::ApplyTemplate);
    connect(&view_, &TimelineDocumentView::SaveTemplateRequested, this,
            &TimelineTemplateController::SaveTemplate);
    connect(&view_, &TimelineDocumentView::UpdateTemplateRequested, this,
            &TimelineTemplateController::UpdateTemplate);
    connect(&view_, &TimelineDocumentView::RenameTemplateRequested, this,
            &TimelineTemplateController::RenameTemplate);
    connect(&view_, &TimelineDocumentView::DuplicateTemplateRequested, this,
            &TimelineTemplateController::DuplicateTemplate);
    connect(&view_, &TimelineDocumentView::DeleteTemplateRequested, this,
            &TimelineTemplateController::DeleteTemplate);
    connect(&view_, &TimelineDocumentView::EditColumnsRequested, this,
            &TimelineTemplateController::EditExportColumns);
    LoadTemplates();
}

void TimelineTemplateController::ApplyTemplate(const QString &identifier) {
    if (identifier.isEmpty()) {
        active_identifier_.reset();
        view_.SetFilterQuery(services::TimelineFilterQuery{});
        RefreshTemplateState();
        return;
    }
    const auto *value = service_.Find(Utf8String(identifier));
    if (value == nullptr) {
        active_identifier_.reset();
        RefreshTemplateState();
        return;
    }
    active_identifier_ = value->identifier;
    event_projection_ = value->event_projection;
    view_.SetFilterQuery(value->filter);
    RefreshTemplateState();
}

void TimelineTemplateController::DeleteTemplate(void) {
    if (!active_identifier_.has_value()) {
        return;
    }
    const auto *value = service_.Find(*active_identifier_);
    if (value == nullptr) {
        active_identifier_.reset();
        RefreshTemplateState();
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
    const auto result = service_.Remove(value->identifier);
    if (!result.has_value()) {
        ShowServiceFailure(tr("Could Not Delete Template"), result.error());
        return;
    }
    active_identifier_.reset();
    RefreshTemplateState();
}

void TimelineTemplateController::DuplicateTemplate(void) {
    if (!active_identifier_.has_value()) {
        return;
    }
    const auto *value = service_.Find(*active_identifier_);
    if (value == nullptr) {
        active_identifier_.reset();
        RefreshTemplateState();
        return;
    }
    const auto name = PromptForTemplateName(
        tr("Duplicate Template"), tr("Name for the copy:"),
        tr("%1 copy").arg(Utf8(value->name)));
    if (!name.has_value()) {
        return;
    }
    const auto result = service_.Duplicate(value->identifier, *name);
    if (!result.has_value()) {
        ShowServiceFailure(tr("Could Not Duplicate Template"), result.error());
        return;
    }
    active_identifier_ = result->identifier;
    RefreshTemplateState();
}

void TimelineTemplateController::EditExportColumns(void) {
    EventProjectionDialog dialog{event_projection_, &window_};
    if (dialog.exec() == QDialog::Accepted) {
        SetEventProjection(dialog.Projection());
    }
}

std::span<const core::TimelineEventField>
TimelineTemplateController::EventProjection(void) const noexcept {
    return event_projection_;
}

void TimelineTemplateController::LoadTemplates(void) {
    const auto result = service_.Load();
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
    if (auto *buttons = dialog.findChild<QDialogButtonBox *>();
        buttons != nullptr) {
        if (auto *accept = buttons->button(QDialogButtonBox::Ok);
            accept != nullptr) {
            SetAutomationIdentifier(*accept, u"acceptTemplateNameButton");
        }
        if (auto *cancel = buttons->button(QDialogButtonBox::Cancel);
            cancel != nullptr) {
            SetAutomationIdentifier(*cancel, u"cancelTemplateNameButton");
        }
    }
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
    bool modified = false;
    if (active_identifier_.has_value()) {
        const auto *value = service_.Find(*active_identifier_);
        if (value == nullptr) {
            active_identifier_.reset();
        } else {
            modified = value->filter != filter_ ||
                       value->event_projection != event_projection_;
        }
    }
    view_.SetTemplates(service_.Templates(), active_identifier_, modified);
}

void TimelineTemplateController::RenameTemplate(void) {
    if (!active_identifier_.has_value()) {
        return;
    }
    const auto *value = service_.Find(*active_identifier_);
    if (value == nullptr) {
        active_identifier_.reset();
        RefreshTemplateState();
        return;
    }
    const auto identifier = value->identifier;
    const auto previous_name = value->name;
    const auto name = PromptForTemplateName(
        tr("Rename Template"), tr("Template name:"), Utf8(previous_name));
    if (!name.has_value() || *name == previous_name) {
        return;
    }
    const auto result = service_.Rename(identifier, *name);
    if (!result.has_value()) {
        ShowServiceFailure(tr("Could Not Rename Template"), result.error());
        return;
    }
    RefreshTemplateState();
}

void TimelineTemplateController::RestoreForTimeline(void) {
    if (active_identifier_.has_value()) {
        const auto *value = service_.Find(*active_identifier_);
        if (value != nullptr) {
            event_projection_ = value->event_projection;
            view_.SetFilterQuery(value->filter);
            return;
        }
        active_identifier_.reset();
    }
    filter_ = view_.FilterQuery();
    event_projection_ = DefaultEventProjection();
    RefreshTemplateState();
}

void TimelineTemplateController::SaveTemplate(void) {
    if (!filter_valid_) {
        ShowWarning(
            window_, tr("Could Not Save Template"),
            tr("Fix the invalid filter condition before saving a template."),
            QStringLiteral("invalidTemplateFilterDialog"));
        return;
    }
    const auto name = PromptForTemplateName(
        tr("Save Filter and Export Template"), tr("Template name:"), {});
    if (!name.has_value()) {
        return;
    }
    const auto result = service_.Create(*name, filter_, event_projection_);
    if (!result.has_value()) {
        ShowServiceFailure(tr("Could Not Save Template"), result.error());
        return;
    }
    active_identifier_ = result->identifier;
    RefreshTemplateState();
}

void TimelineTemplateController::SetEventProjection(
    std::vector<core::TimelineEventField> event_projection) {
    event_projection_ = std::move(event_projection);
    RefreshTemplateState();
}

void TimelineTemplateController::SetFilterState(
    const services::TimelineFilterQuery &filter, bool valid) {
    filter_ = filter;
    filter_valid_ = valid;
    RefreshTemplateState();
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
    if (!active_identifier_.has_value() || !filter_valid_) {
        if (!filter_valid_) {
            ShowWarning(
                window_, tr("Could Not Update Template"),
                tr("Fix the invalid filter condition before updating the "
                   "template."),
                QStringLiteral("invalidTemplateFilterDialog"));
        }
        return;
    }
    const auto result =
        service_.Update(*active_identifier_, filter_, event_projection_);
    if (!result.has_value()) {
        ShowServiceFailure(tr("Could Not Update Template"), result.error());
        return;
    }
    RefreshTemplateState();
}

} // namespace edit_atlas::app
