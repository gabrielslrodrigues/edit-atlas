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

#ifndef EDIT_ATLAS_SERVICES_TIMELINE_RENDERED_VIDEO_EXPORT_SERVICE_HPP_
#define EDIT_ATLAS_SERVICES_TIMELINE_RENDERED_VIDEO_EXPORT_SERVICE_HPP_

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <stop_token>
#include <string_view>
#include <vector>

#include "edit_atlas/core/editorial_timeline.hpp"
#include "edit_atlas/core/format_registry.hpp"
#include "edit_atlas/media/video_decoder.hpp"
#include "edit_atlas/services/timeline_document_export_service.hpp"
#include "edit_atlas/services/timeline_frame_extraction_service.hpp"
#include "edit_atlas/services/timeline_video_inspection_service.hpp"

namespace edit_atlas::services {

namespace timeline_rendered_video_export_diagnostic_code {

/// The selected projection requires a rendered-video input.
inline constexpr std::string_view kVideoRequired = "video.input_required";

}  // namespace timeline_rendered_video_export_diagnostic_code

/// Input for an export that may require frames from a rendered video.
struct TimelineRenderedVideoExportRequest final {
  /// Normal timeline document export request.
  TimelineDocumentExportRequest document_export;
  /// Complete imported timeline used to validate rendered-video timing.
  core::TimelineDocument reference_timeline;
  /// Optional rendered-video path required by image-bearing projections.
  std::optional<std::filesystem::path> video_path;
};

/// Identifies the stage at which a rendered-video export failed.
enum class TimelineRenderedVideoExportFailureKind {
  /// The selected projection requires a video, but none was supplied.
  kVideoRequired,
  /// The rendered video could not be opened or validated.
  kVideoInspectionFailed,
  /// Initial event frames could not be extracted.
  kFrameExtractionFailed,
  /// The prepared document could not be exported.
  kDocumentExportFailed,
};

/// Failure from rendered-video preparation or document export.
struct TimelineRenderedVideoExportFailure final {
  /// Stage at which the operation failed.
  TimelineRenderedVideoExportFailureKind kind;
  /// Candidate video path, when video preparation was attempted.
  std::optional<std::filesystem::path> video_path;
  /// Decoder details supplied by media inspection or extraction.
  std::optional<media::VideoDecoderFailure> decoder_failure;
  /// Underlying document export failure, when that stage was reached.
  std::optional<TimelineDocumentExportFailure> document_export_failure;
  /// Structured diagnostics describing the failure.
  std::vector<core::Diagnostic> diagnostics;
};

/// Successful document export and optional rendered-video details.
struct TimelineRenderedVideoExportReceipt final {
  /// Receipt from the committed document export.
  TimelineDocumentExportReceipt document_export;
  /// Inspected video information when initial frames were requested.
  std::optional<media::VideoMediaInfo> video_information;
  /// Validated record-to-video mapping when a video was used.
  std::optional<TimelineVideoMapping> video_mapping;
  /// Number of distinct video frames decoded for the export.
  std::size_t unique_frame_count;
};

/// Either a committed export or a rendered-video-aware failure.
using TimelineRenderedVideoExportResult =
    std::expected<TimelineRenderedVideoExportReceipt,
                  TimelineRenderedVideoExportFailure>;

/// Coordinates optional rendered-video preparation with document export.
class TimelineRenderedVideoExportService final {
 public:
  /// Creates the service from a registry that must outlive it.
  explicit TimelineRenderedVideoExportService(
      const core::FormatRegistry& registry) noexcept;

  /// Services are non-copyable and non-movable: the owned document-export
  /// service holds a reference that cannot be reseated.
  TimelineRenderedVideoExportService(
      const TimelineRenderedVideoExportService&) = delete;
  TimelineRenderedVideoExportService& operator=(
      const TimelineRenderedVideoExportService&) = delete;
  TimelineRenderedVideoExportService(TimelineRenderedVideoExportService&&) =
      delete;
  TimelineRenderedVideoExportService& operator=(
      TimelineRenderedVideoExportService&&) = delete;

  /// Validates video timing, extracts selected event frames, and exports.
  ///
  /// Video validation always uses the complete `reference_timeline`, while
  /// extraction uses the possibly filtered `document_export.timeline`.
  [[nodiscard]] TimelineRenderedVideoExportResult Export(
      TimelineRenderedVideoExportRequest request,
      std::stop_token stop_token = {},
      const TimelineFrameExtractionProgressCallback& progress = {}) const;

 private:
  TimelineDocumentExportService document_export_service_;
  TimelineVideoInspectionService video_inspection_service_;
  TimelineFrameExtractionService frame_extraction_service_;
};

}  // namespace edit_atlas::services

#endif  // EDIT_ATLAS_SERVICES_TIMELINE_RENDERED_VIDEO_EXPORT_SERVICE_HPP_
