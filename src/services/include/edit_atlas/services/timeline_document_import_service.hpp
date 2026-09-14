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

#ifndef EDIT_ATLAS_SERVICES_TIMELINE_DOCUMENT_IMPORT_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_DOCUMENT_IMPORT_SERVICE_HPP_

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "edit_atlas/core/editorial_timeline.hpp"
#include "edit_atlas/core/format_registry.hpp"
#include "edit_atlas/core/timeline_document_pipeline.hpp"

namespace edit_atlas::services {

/// Describes one request to import a local timeline.
struct TimelineDocumentImportRequest final {
  /// The local source path to read.
  std::filesystem::path path;
  /// An explicit stable importer identifier, or no value to probe content.
  std::optional<std::string> format_identifier;
  /// Format-specific import options.
  std::vector<core::MetadataEntry> options;
};

/// Identifies the stage at which importing a document failed.
enum class TimelineDocumentImportFailureKind {
  /// The source file could not be opened.
  kOpenFailed,
  /// The opened source file could not be read completely.
  kReadFailed,
  /// The format-independent pipeline could not import the source.
  kImportFailed,
};

/// Contains a successfully imported timeline and its source context.
struct TimelineDocumentImportReceipt final {
  /// The imported source path.
  std::filesystem::path path;
  /// The complete imported timeline.
  core::TimelineDocument timeline;
  /// Non-fatal diagnostics produced during import.
  std::vector<core::Diagnostic> diagnostics;
};

/// Contains a presentation-neutral filesystem or importer failure.
struct TimelineDocumentImportFailure final {
  /// The requested source path.
  std::filesystem::path path;
  /// The stage at which import failed.
  TimelineDocumentImportFailureKind kind;
  /// The native filesystem error, when failure involved the filesystem.
  std::error_code filesystem_error;
  /// Structured diagnostics produced by the pipeline or importer.
  std::vector<core::Diagnostic> diagnostics;
};

/// Either an imported timeline receipt or a presentation-neutral failure.
using TimelineDocumentImportResult =
    std::expected<TimelineDocumentImportReceipt, TimelineDocumentImportFailure>;

/// Coordinates filesystem input with the format-independent import pipeline.
///
/// The registry must outlive the service. Scheduling and user interaction are
/// responsibilities of the calling frontend.
class TimelineDocumentImportService final {
 public:
  /// Creates a service backed by a registry that must outlive it.
  explicit TimelineDocumentImportService(
      const core::FormatRegistry& registry) noexcept;

  /// Services are non-copyable and non-movable: the owned pipeline holds a
  /// reference that cannot be reseated.
  TimelineDocumentImportService(const TimelineDocumentImportService&) = delete;
  TimelineDocumentImportService& operator=(
      const TimelineDocumentImportService&) = delete;
  TimelineDocumentImportService(TimelineDocumentImportService&&) = delete;
  TimelineDocumentImportService& operator=(TimelineDocumentImportService&&) =
      delete;

  /// Reads and imports the local timeline described by \p request.
  ///
  /// \returns A receipt on success or a stage-specific failure.
  [[nodiscard]] TimelineDocumentImportResult Import(
      TimelineDocumentImportRequest request) const;

 private:
  core::TimelineDocumentPipeline pipeline_;
};

}  // namespace edit_atlas::services

#endif  // EDIT_ATLAS_SERVICES_TIMELINE_DOCUMENT_IMPORT_SERVICE_HPP_
