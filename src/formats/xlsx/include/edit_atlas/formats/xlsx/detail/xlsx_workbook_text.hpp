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

#ifndef EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_
#define EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_

#include <span>
#include <string_view>

#include "edit_atlas/core/timeline_projection.hpp"
#include "edit_atlas/formats/xlsx/xlsx_exporter.hpp"

namespace edit_atlas::formats::xlsx::detail {

/// Identifies a localized XLSX label that is not an event projection field.
enum class WorkbookTextKey {
  kEventsSheet,
  kTimelineSheet,
  kDiagnosticsSheet,
  kSubject,
  kCategory,
  kKeywords,
  kComments,
  kTitle,
  kFrameRate,
  kTimecodeMode,
  kDropFrame,
  kNonDropFrame,
  kEventCount,
  kVideo,
  kAudio,
  kData,
  kOther,
  kCut,
  kDissolve,
  kWipe,
  kKey,
  kInfo,
  kWarning,
  kError,
  kTimelinePropertyColumn,
  kTimelineValueColumn,
  kDiagnosticSeverityColumn,
  kDiagnosticCodeColumn,
  kDiagnosticMessageColumn,
  kDiagnosticSourceColumn,
  kDiagnosticLineColumn,
  kDiagnosticColumnColumn,
  kCount,
};

/// Resolves localized labels and headers for one workbook language.
class WorkbookText final {
 public:
  explicit constexpr WorkbookText(WorkbookLanguage language) noexcept
      : language_(language) {}

  /// Returns the localized text for \p key.
  [[nodiscard]] std::string_view Get(WorkbookTextKey key) const noexcept;

  /// Returns the localized header for an event projection field.
  [[nodiscard]] std::string_view EventColumn(
      core::TimelineEventField field) const noexcept;

  /// Returns the ordered timeline-sheet header keys.
  [[nodiscard]] std::span<const WorkbookTextKey> TimelineColumns(
      void) const noexcept;

  /// Returns the ordered diagnostics-sheet header keys.
  [[nodiscard]] std::span<const WorkbookTextKey> DiagnosticColumns(
      void) const noexcept;

 private:
  WorkbookLanguage language_;
};

[[nodiscard]] const WorkbookText& WorkbookTextFor(
    WorkbookLanguage language) noexcept;

}  // namespace edit_atlas::formats::xlsx::detail

#endif  // EDIT_ATLAS_FORMATS_XLSX_DETAIL_XLSX_WORKBOOK_TEXT_HPP_
