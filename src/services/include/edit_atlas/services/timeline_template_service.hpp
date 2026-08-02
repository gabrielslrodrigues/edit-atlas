#ifndef EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_SERVICE_HPP_

#include <edit_atlas/core/timeline_projection.hpp>

#include <edit_atlas/services/timeline_filter.hpp>
#include <edit_atlas/services/timeline_template.hpp>

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace edit_atlas::services {

/// A recoverable problem affecting one persisted template file.
struct TimelineTemplateLoadDiagnostic final {
    /// File that could not be interpreted as a supported template.
    std::filesystem::path path;
    /// Human-readable, non-localized technical detail.
    std::string message;
};

/// Identifies why a template operation could not be completed.
enum class TimelineTemplateFailureKind {
    /// The template catalog has not been loaded.
    kNotLoaded,
    /// The requested template does not exist.
    kNotFound,
    /// The proposed template data is invalid.
    kInvalidTemplate,
    /// Another template already has the proposed name.
    kNameConflict,
    /// Local persistence failed.
    kStorageFailed,
};

/// Describes a presentation-neutral template operation failure.
struct TimelineTemplateFailure final {
    /// Reason the operation failed.
    TimelineTemplateFailureKind kind;
    /// Relevant file or directory, when available.
    std::filesystem::path path;
    /// Native filesystem error, when available.
    std::error_code filesystem_error;
    /// Human-readable, non-localized technical detail.
    std::string message;
};

/// Successful catalog loading and recoverable per-file diagnostics.
using TimelineTemplateLoadResult =
    std::expected<std::vector<TimelineTemplateLoadDiagnostic>,
                  TimelineTemplateFailure>;

/// A created or updated template, or a structured failure.
using TimelineTemplateResult =
    std::expected<TimelineTemplate, TimelineTemplateFailure>;

/// A successful template mutation or a structured failure.
using TimelineTemplateMutationResult =
    std::expected<void, TimelineTemplateFailure>;

/// Owns and persists the reusable timeline-template catalog.
class TimelineTemplateService final {
  public:
    /// Creates a service backed by \p directory.
    explicit TimelineTemplateService(std::filesystem::path directory);

    /// Loads the persisted catalog, skipping invalid individual files.
    [[nodiscard]] TimelineTemplateLoadResult Load(void);

    /// Returns the loaded templates in deterministic name order.
    [[nodiscard]] std::span<const TimelineTemplate> Templates(void) const;

    /// Returns a loaded template by stable identifier, or null when absent.
    [[nodiscard]] const TimelineTemplate *
    Find(std::string_view identifier) const;

    /// Creates and persists a new template from current frontend state.
    [[nodiscard]] TimelineTemplateResult
    Create(std::string name, TimelineFilterQuery filter,
           std::vector<core::TimelineEventField> event_projection);

    /// Replaces the saved filter and projection of an existing template.
    [[nodiscard]] TimelineTemplateResult
    Update(std::string_view identifier, TimelineFilterQuery filter,
           std::vector<core::TimelineEventField> event_projection);

    /// Changes an existing template name without changing its contents.
    [[nodiscard]] TimelineTemplateResult Rename(std::string_view identifier,
                                                std::string name);

    /// Copies the saved contents of an existing template under a new name.
    [[nodiscard]] TimelineTemplateResult Duplicate(std::string_view identifier,
                                                   std::string name);

    /// Removes an existing template from persistence and the catalog.
    [[nodiscard]] TimelineTemplateMutationResult
    Remove(std::string_view identifier);

  private:
    [[nodiscard]] std::expected<void, TimelineTemplateFailure>
    ValidateName(std::string_view name,
                 std::string_view excluded_identifier = {}) const;
    [[nodiscard]] TimelineTemplate *FindMutable(std::string_view identifier);
    void Sort(void);

    std::filesystem::path directory_;
    std::vector<TimelineTemplate> templates_;
    bool loaded_ = false;
};

} // namespace edit_atlas::services

#endif // EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_SERVICE_HPP_
