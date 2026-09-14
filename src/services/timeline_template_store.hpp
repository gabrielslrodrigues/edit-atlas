// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_STORE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_STORE_HPP_

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "edit_atlas/services/timeline_template.hpp"

namespace edit_atlas::services {

/// Current on-disk schema written by the timeline template store.
inline constexpr int kTimelineTemplateSchemaVersion = 1;

/// A recoverable problem affecting one persisted template file.
struct TimelineTemplateStoreLoadDiagnostic final {
  /// File that could not be interpreted as a supported template.
  std::filesystem::path path;
  /// Human-readable, non-localized technical detail.
  std::string message;

  /// Compares the file and diagnostic detail.
  bool operator==(const TimelineTemplateStoreLoadDiagnostic&) const = default;
};

/// Valid templates and recoverable per-file loading diagnostics.
struct TimelineTemplateStoreLoadResult final {
  /// Templates successfully loaded from the store.
  std::vector<TimelineTemplate> templates;
  /// Invalid or unsupported files skipped during loading.
  std::vector<TimelineTemplateStoreLoadDiagnostic> diagnostics;
};

/// Identifies a store-wide filesystem or serialization failure.
enum class TimelineTemplateStoreFailureKind {
  /// The template directory could not be inspected or created.
  kStorageUnavailable,
  /// A valid template could not be serialized.
  kSerializationFailed,
  /// A temporary template file could not be written.
  kWriteFailed,
  /// A completed template file could not be committed.
  kCommitFailed,
  /// A persisted template file could not be removed.
  kRemoveFailed,
};

/// Describes an operation that could not be completed by the template store.
struct TimelineTemplateStoreFailure final {
  /// Operation stage that failed.
  TimelineTemplateStoreFailureKind kind;
  /// Relevant file or directory.
  std::filesystem::path path;
  /// Native filesystem error, when available.
  std::error_code filesystem_error;
  /// Human-readable, non-localized technical detail.
  std::string message;
};

/// Loaded templates or a store-wide failure.
using TimelineTemplateLoadOutcome =
    std::expected<TimelineTemplateStoreLoadResult,
                  TimelineTemplateStoreFailure>;

/// A successful mutation or its store-wide failure.
using TimelineTemplateMutationOutcome =
    std::expected<void, TimelineTemplateStoreFailure>;

/// Persists independent, versioned template files below one local directory.
class TimelineTemplateStore final {
 public:
  /// Creates a store rooted at \p directory.
  explicit TimelineTemplateStore(std::filesystem::path directory);

  /// Loads every valid template while reporting invalid files separately.
  [[nodiscard]] TimelineTemplateLoadOutcome Load(void) const;

  /// Atomically writes or replaces \p value by its stable identifier.
  [[nodiscard]] TimelineTemplateMutationOutcome Save(
      const TimelineTemplate& value) const;

  /// Removes the template identified by \p identifier.
  [[nodiscard]] TimelineTemplateMutationOutcome Remove(
      std::string_view identifier) const;

 private:
  std::filesystem::path directory_;
};

}  // namespace edit_atlas::services

#endif  // EDIT_ATLAS_SERVICES_TIMELINE_TEMPLATE_STORE_HPP_
