#include "timeline_template_controller.hpp"

#include <edit_atlas/app/timeline_document_view.hpp>

#include "event_projection_dialog.hpp"

#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_template_service.hpp>

#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStandardPaths>
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

[[nodiscard]] std::vector<core::TimelineEventField>
DefaultEventProjection(void) {
    const auto fields = core::DefaultTimelineEventProjection();
    return {fields.begin(), fields.end()};
}

[[nodiscard]] std::filesystem::path TemplateDirectory(void) {
    return FilesystemPath(QStandardPaths::writableLocation(
               QStandardPaths::AppLocalDataLocation)) /
           "templates";
}

} // namespace

TimelineTemplateController::TimelineTemplateController(
    TimelineDocumentView &view, QWidget &window, QObject *parent)
    : QObject{parent}, view_{view}, window_{window},
      service_{TemplateDirectory()},
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
    if (QMessageBox::question(
            &window_, tr("Delete Template?"),
            tr("Delete the template “%1”? This cannot be undone.")
                .arg(Utf8(value->name)),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) {
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
    message.addButton(tr("Close"), QMessageBox::RejectRole);
    message.exec();
}

std::optional<std::string> TimelineTemplateController::PromptForTemplateName(
    const QString &title, const QString &label, const QString &initial) {
    bool accepted = false;
    auto proposed = QInputDialog::getText(&window_, title, label,
                                          QLineEdit::Normal, initial, &accepted)
                        .trimmed();
    if (!accepted) {
        return std::nullopt;
    }
    if (proposed.isEmpty()) {
        QMessageBox::warning(&window_, tr("Invalid Template Name"),
                             tr("Enter a name for the template."));
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
        QMessageBox::warning(
            &window_, tr("Could Not Save Template"),
            tr("Fix the invalid filter condition before saving a template."));
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
    message.addButton(tr("Close"), QMessageBox::RejectRole);
    message.exec();
}

void TimelineTemplateController::UpdateTemplate(void) {
    if (!active_identifier_.has_value() || !filter_valid_) {
        if (!filter_valid_) {
            QMessageBox::warning(
                &window_, tr("Could Not Update Template"),
                tr("Fix the invalid filter condition before updating the "
                   "template."));
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
