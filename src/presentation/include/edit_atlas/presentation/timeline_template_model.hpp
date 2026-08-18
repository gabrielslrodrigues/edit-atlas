#ifndef EDIT_ATLAS_PRESENTATION_TIMELINE_TEMPLATE_MODEL_HPP_
#define EDIT_ATLAS_PRESENTATION_TIMELINE_TEMPLATE_MODEL_HPP_

#include <edit_atlas/services/timeline_template.hpp>

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QModelIndex>
#include <QObject>
#include <QVariant>
#include <Qt>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace edit_atlas::presentation {

/// Presents saved timeline templates and the explicit no-template choice.
class TimelineTemplateModel final : public QAbstractListModel {
    Q_OBJECT

  public:
    /// Roles describing one template choice.
    enum Role {
        /// Stable, nonlocalized template identifier.
        kIdentifierRole = Qt::UserRole,
        /// Whether this choice is currently active.
        kActiveRole,
        /// Whether the active template has unapplied changes.
        kModifiedRole,
    };
    Q_ENUM(Role)

    /// Creates a model containing only the no-template choice.
    explicit TimelineTemplateModel(QObject *parent = nullptr);
    /// Destroys the copied template catalog.
    ~TimelineTemplateModel(void) override = default;

    TimelineTemplateModel(const TimelineTemplateModel &) = delete;
    TimelineTemplateModel &operator=(const TimelineTemplateModel &) = delete;
    TimelineTemplateModel(TimelineTemplateModel &&) = delete;
    TimelineTemplateModel &operator=(TimelineTemplateModel &&) = delete;

    /// Returns the no-template row plus every saved template.
    [[nodiscard]] int
    rowCount(const QModelIndex &parent = QModelIndex{}) const override;
    /// Returns localized display text and stable template state.
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    /// Returns stable QML role names for template state.
    [[nodiscard]] QHash<int, QByteArray> roleNames(void) const override;

    /// Replaces the copied catalog, active identifier, and modified state.
    void SetTemplates(std::span<const services::TimelineTemplate> templates,
                      std::optional<std::string_view> active_identifier,
                      bool modified);
    /// Returns the row containing the active choice.
    [[nodiscard]] int ActiveRow(void) const noexcept;
    /// Notifies views that localized no-template text changed.
    void Retranslate(void);

  private:
    struct TemplateItem final {
        std::string identifier;
        std::string name;
    };

    std::vector<TemplateItem> templates_;
    std::string active_identifier_;
    bool modified_ = false;
};

} // namespace edit_atlas::presentation

#endif // EDIT_ATLAS_PRESENTATION_TIMELINE_TEMPLATE_MODEL_HPP_
