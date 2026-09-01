#include <edit_atlas/frontends/quick/timeline_configuration_view_model.hpp>

#include <edit_atlas/presentation/timeline_document_view_model.hpp>
#include <edit_atlas/presentation/timeline_event_projection_model.hpp>
#include <edit_atlas/presentation/timeline_filter_model.hpp>
#include <edit_atlas/presentation/timeline_template_model.hpp>
#include <edit_atlas/presentation/timeline_template_view_model.hpp>

#include <edit_atlas/services/timeline_template_service.hpp>

#include <QAbstractItemModel>
#include <QByteArray>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <variant>

namespace edit_atlas::frontends::quick {
namespace {

[[nodiscard]] std::string Utf8String(const QString &text) {
    const auto utf8 = text.toUtf8();
    return {utf8.constData(), static_cast<std::size_t>(utf8.size())};
}

[[nodiscard]] QString Utf8Text(const std::string &text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

TimelineConfigurationViewModel::TimelineConfigurationViewModel(
    presentation::TimelineDocumentViewModel &document_view_model,
    std::filesystem::path template_directory, QObject *parent)
    : QObject{parent}, document_view_model_{document_view_model},
      template_view_model_{std::move(template_directory)} {
    connect(&document_view_model_,
            &presentation::TimelineDocumentViewModel::filterChanged, this,
            [this] {
                if (synchronizing_template_state_) {
                    return;
                }
                template_view_model_.SetFilterState(
                    filter_model_.Query(),
                    document_view_model_.FilterError() == nullptr);
                emit filterStateChanged();
                emit templateStateChanged();
            });
    connect(&filter_model_, &presentation::TimelineFilterModel::queryChanged,
            this, &TimelineConfigurationViewModel::HandleFilterQueryChanged);
    connect(&event_projection_model_,
            &presentation::TimelineEventProjectionModel::projectionChanged,
            this,
            &TimelineConfigurationViewModel::HandleEventProjectionChanged);
    connect(&template_view_model_,
            &presentation::TimelineTemplateViewModel::templatesChanged, this,
            &TimelineConfigurationViewModel::HandleTemplateStateChanged);
    connect(&template_view_model_,
            &presentation::TimelineTemplateViewModel::activeTemplateChanged,
            this, &TimelineConfigurationViewModel::HandleTemplateStateChanged);
    connect(&template_view_model_,
            &presentation::TimelineTemplateViewModel::modifiedChanged, this,
            &TimelineConfigurationViewModel::HandleTemplateStateChanged);
    connect(&template_view_model_,
            &presentation::TimelineTemplateViewModel::filterStateChanged, this,
            &TimelineConfigurationViewModel::SynchronizeTemplateState);
    connect(&template_view_model_,
            &presentation::TimelineTemplateViewModel::eventProjectionChanged,
            this, &TimelineConfigurationViewModel::SynchronizeTemplateState);
    static_cast<void>(template_view_model_.Load());
    SynchronizeTemplateState();
}

QAbstractItemModel *TimelineConfigurationViewModel::FilterModel(void) noexcept {
    return &filter_model_;
}

QAbstractItemModel *
TimelineConfigurationViewModel::TemplateModel(void) noexcept {
    return &template_view_model_.TemplateModel();
}

QAbstractItemModel *
TimelineConfigurationViewModel::EventProjectionModel(void) noexcept {
    return &event_projection_model_;
}

int TimelineConfigurationViewModel::ActiveTemplateRow(void) const noexcept {
    return template_view_model_.TemplateModel().ActiveRow();
}

bool TimelineConfigurationViewModel::HasActiveTemplate(void) const noexcept {
    return template_view_model_.ActiveIdentifier().has_value();
}

bool TimelineConfigurationViewModel::IsTemplateModified(void) const noexcept {
    if (!HasActiveTemplate()) {
        return false;
    }
    const auto projection = event_projection_model_.Projection();
    return template_view_model_.IsModified() ||
           !std::ranges::equal(projection,
                               template_view_model_.EventProjection());
}

QString TimelineConfigurationViewModel::ActiveTemplateName(void) const {
    const auto row = ActiveTemplateRow();
    return HasActiveTemplate()
               ? template_view_model_.TemplateModel()
                     .data(template_view_model_.TemplateModel().index(row, 0))
                     .toString()
               : QString{};
}

QString TimelineConfigurationViewModel::TemplateOperationErrorText(void) const {
    return template_operation_error_text_;
}

bool TimelineConfigurationViewModel::IsFilterValid(void) const noexcept {
    return template_view_model_.IsFilterValid();
}

QString TimelineConfigurationViewModel::FilterErrorText(void) const {
    const auto *error = document_view_model_.FilterError();
    return error == nullptr
               ? QString{}
               : tr("Condition %1 has an invalid regular expression: %2")
                     .arg(static_cast<qulonglong>(error->condition_index + 1))
                     .arg(Utf8Text(error->message));
}

bool TimelineConfigurationViewModel::IsEventProjectionValid(
    void) const noexcept {
    return event_projection_model_.IsValid();
}

int TimelineConfigurationViewModel::EventProjectionSelectedCount(
    void) const noexcept {
    return event_projection_model_.SelectedCount();
}

int TimelineConfigurationViewModel::FilterCombination(void) const noexcept {
    return filter_model_.Combination();
}

void TimelineConfigurationViewModel::SetFilterCombination(int combination) {
    filter_model_.SetCombination(combination);
}

QStringList TimelineConfigurationViewModel::FilterCombinationNames(void) const {
    return filter_model_.CombinationNames();
}

QStringList TimelineConfigurationViewModel::FilterFieldNames(void) const {
    return filter_model_.FieldNames();
}

QStringList TimelineConfigurationViewModel::FilterTrackKindNames(void) const {
    return filter_model_.TrackKindNames();
}

QStringList TimelineConfigurationViewModel::FilterEditTypeNames(void) const {
    return filter_model_.EditTypeNames();
}

int TimelineConfigurationViewModel::TextFilterEditor(void) const noexcept {
    return static_cast<int>(presentation::TimelineFilterEditor::kText);
}

int TimelineConfigurationViewModel::TrackKindFilterEditor(void) const noexcept {
    return static_cast<int>(presentation::TimelineFilterEditor::kTrackKind);
}

int TimelineConfigurationViewModel::EditTypeFilterEditor(void) const noexcept {
    return static_cast<int>(presentation::TimelineFilterEditor::kEditType);
}

int TimelineConfigurationViewModel::TimecodeFilterEditor(void) const noexcept {
    return static_cast<int>(presentation::TimelineFilterEditor::kTimecode);
}

int TimelineConfigurationViewModel::DurationFilterEditor(void) const noexcept {
    return static_cast<int>(presentation::TimelineFilterEditor::kDuration);
}

void TimelineConfigurationViewModel::RestoreForTimeline(void) {
    template_view_model_.RestoreForTimeline();
    SynchronizeTemplateState();
}

void TimelineConfigurationViewModel::Retranslate(void) {
    filter_model_.Retranslate();
    template_view_model_.Retranslate();
    event_projection_model_.Retranslate();
    emit displayTextChanged();
    emit filterStateChanged();
}

void TimelineConfigurationViewModel::AddFilterCondition(void) {
    filter_model_.AddCondition();
}

void TimelineConfigurationViewModel::RemoveFilterCondition(int row) {
    filter_model_.RemoveCondition(row);
}

void TimelineConfigurationViewModel::ClearFilter(void) {
    filter_model_.Clear();
}

void TimelineConfigurationViewModel::SelectTemplateRow(int row) {
    const auto index = template_view_model_.TemplateModel().index(row, 0);
    if (!index.isValid()) {
        return;
    }
    const auto identifier =
        template_view_model_.TemplateModel()
            .data(index, presentation::TimelineTemplateModel::kIdentifierRole)
            .toString();
    if (identifier.isEmpty()) {
        template_view_model_.SelectNoTemplate();
        return;
    }
    static_cast<void>(
        template_view_model_.SelectTemplate(Utf8String(identifier)));
}

bool TimelineConfigurationViewModel::CreateTemplate(const QString &name) {
    if (!event_projection_model_.IsValid()) {
        template_operation_error_text_ =
            tr("Select at least one event column before saving a template.");
        emit templateOperationErrorChanged();
        return false;
    }
    return HandleTemplateCommandResult(
        template_view_model_.Create(Utf8String(name.trimmed())));
}

bool TimelineConfigurationViewModel::UpdateActiveTemplate(void) {
    if (!event_projection_model_.IsValid()) {
        template_operation_error_text_ =
            tr("Select at least one event column before updating the "
               "template.");
        emit templateOperationErrorChanged();
        return false;
    }
    return HandleTemplateCommandResult(template_view_model_.UpdateActive());
}

bool TimelineConfigurationViewModel::RenameActiveTemplate(const QString &name) {
    return HandleTemplateCommandResult(
        template_view_model_.RenameActive(Utf8String(name.trimmed())));
}

bool TimelineConfigurationViewModel::DuplicateActiveTemplate(
    const QString &name) {
    return HandleTemplateCommandResult(
        template_view_model_.DuplicateActive(Utf8String(name.trimmed())));
}

bool TimelineConfigurationViewModel::RemoveActiveTemplate(void) {
    return HandleTemplateCommandResult(template_view_model_.RemoveActive());
}

void TimelineConfigurationViewModel::ClearTemplateOperationError(void) {
    if (template_operation_error_text_.isEmpty()) {
        return;
    }
    template_operation_error_text_.clear();
    emit templateOperationErrorChanged();
}

void TimelineConfigurationViewModel::SetEventProjectionSelected(int row,
                                                                bool selected) {
    event_projection_model_.SetSelected(row, selected);
}

void TimelineConfigurationViewModel::MoveEventProjectionUp(int row) {
    event_projection_model_.MoveUp(row);
}

void TimelineConfigurationViewModel::MoveEventProjectionDown(int row) {
    event_projection_model_.MoveDown(row);
}

void TimelineConfigurationViewModel::MoveEventProjection(int source_row,
                                                         int destination_row) {
    event_projection_model_.Move(source_row, destination_row);
}

void TimelineConfigurationViewModel::HandleFilterQueryChanged(void) {
    if (synchronizing_template_state_) {
        return;
    }
    synchronizing_template_state_ = true;
    auto query = filter_model_.Query();
    document_view_model_.SetFilterQuery(query);
    template_view_model_.SetFilterState(
        std::move(query), document_view_model_.FilterError() == nullptr);
    synchronizing_template_state_ = false;
    emit filterStateChanged();
    emit templateStateChanged();
}

void TimelineConfigurationViewModel::HandleTemplateStateChanged(void) {
    emit templateStateChanged();
}

void TimelineConfigurationViewModel::HandleEventProjectionChanged(void) {
    if (synchronizing_template_state_) {
        return;
    }
    const auto projection = event_projection_model_.Projection();
    if (event_projection_model_.IsValid()) {
        synchronizing_template_state_ = true;
        static_cast<void>(document_view_model_.SetEventProjection(projection));
        static_cast<void>(template_view_model_.SetEventProjection(projection));
        synchronizing_template_state_ = false;
    }
    emit eventProjectionStateChanged();
    emit templateStateChanged();
}

void TimelineConfigurationViewModel::SynchronizeTemplateState(void) {
    if (synchronizing_template_state_) {
        return;
    }
    synchronizing_template_state_ = true;
    filter_model_.SetQuery(template_view_model_.FilterQuery());
    document_view_model_.SetFilterQuery(template_view_model_.FilterQuery());
    static_cast<void>(event_projection_model_.SetProjection(
        template_view_model_.EventProjection()));
    static_cast<void>(document_view_model_.SetEventProjection(
        event_projection_model_.Projection()));
    synchronizing_template_state_ = false;
    emit filterStateChanged();
    emit eventProjectionStateChanged();
    emit templateStateChanged();
}

bool TimelineConfigurationViewModel::HandleTemplateCommandResult(
    presentation::TimelineTemplateCommandResult result) {
    if (result.has_value()) {
        ClearTemplateOperationError();
        return true;
    }
    if (const auto *service_failure =
            std::get_if<services::TimelineTemplateFailure>(&result.error());
        service_failure != nullptr) {
        switch (service_failure->kind) {
        case services::TimelineTemplateFailureKind::kInvalidTemplate:
            template_operation_error_text_ = tr("Enter a template name.");
            break;
        case services::TimelineTemplateFailureKind::kNameConflict:
            template_operation_error_text_ =
                tr("A template with that name already exists.");
            break;
        case services::TimelineTemplateFailureKind::kNotLoaded:
        case services::TimelineTemplateFailureKind::kNotFound:
        case services::TimelineTemplateFailureKind::kStorageFailed:
            template_operation_error_text_ =
                tr("The template could not be stored on this computer.");
            break;
        }
    } else {
        switch (std::get<presentation::TimelineTemplateCommandError>(
            result.error())) {
        case presentation::TimelineTemplateCommandError::kInvalidFilter:
            template_operation_error_text_ =
                tr("Fix the invalid filter before saving the template.");
            break;
        case presentation::TimelineTemplateCommandError::kInvalidProjection:
            template_operation_error_text_ =
                tr("Select at least one event column before saving the "
                   "template.");
            break;
        case presentation::TimelineTemplateCommandError::kNoActiveTemplate:
            template_operation_error_text_ =
                tr("Select a saved template before using this action.");
            break;
        }
    }
    emit templateOperationErrorChanged();
    return false;
}

} // namespace edit_atlas::frontends::quick
