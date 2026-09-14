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

#ifndef EDIT_ATLAS_FORMATS_CMX3600_CMX3600_IMPORTER_HPP_
#define EDIT_ATLAS_FORMATS_CMX3600_CMX3600_IMPORTER_HPP_

#include <string_view>

#include "edit_atlas/core/format.hpp"

namespace edit_atlas::formats::cmx3600 {

/// Stable identifier for the CMX 3600 importer.
inline constexpr std::string_view kFormatIdentifier = "cmx-3600";
/// Import option containing an explicit rational frame rate.
inline constexpr std::string_view kFrameRateOption = "cmx3600.frame_rate";

/// Stable diagnostic codes emitted by the CMX 3600 importer.
namespace diagnostic_code {

/// Input was decoded using a legacy single-byte fallback.
inline constexpr std::string_view kEncodingFallback =
    "cmx3600.encoding_fallback";
/// Input could not be decoded as a supported text encoding.
inline constexpr std::string_view kInvalidEncoding = "cmx3600.invalid_encoding";
/// A non-drop-frame document omitted its required frame rate.
inline constexpr std::string_view kMissingFrameRate =
    "cmx3600.missing_frame_rate";
/// The supplied frame-rate option is invalid or unsupported.
inline constexpr std::string_view kInvalidFrameRate =
    "cmx3600.invalid_frame_rate";
/// An event line does not follow the CMX 3600 event grammar.
inline constexpr std::string_view kMalformedEvent = "cmx3600.malformed_event";
/// An event contains an invalid timecode label or range.
inline constexpr std::string_view kInvalidTimecode = "cmx3600.invalid_timecode";
/// A non-empty line has no recognized CMX 3600 meaning.
inline constexpr std::string_view kUnknownContent = "cmx3600.unknown_content";
/// A continuation record does not follow an event.
inline constexpr std::string_view kOrphanRecord = "cmx3600.orphan_record";

}  // namespace diagnostic_code

/// Imports CMX 3600 edit decision lists into the shared timeline model.
///
/// Non-drop-frame files require the `cmx3600.frame_rate` import option with a
/// value such as `24`, `25`, or `30000/1001`. Drop-frame files default to
/// 30000/1001 when the option is omitted.
class Cmx3600Importer final : public core::Importer {
 public:
  /// Creates a CMX 3600 importer with its stable format description.
  Cmx3600Importer(void);
  /// Destroys the importer.
  ~Cmx3600Importer(void) override = default;

  [[nodiscard]] const core::FormatDescriptor& descriptor(
      void) const noexcept override;

  [[nodiscard]] core::ProbeConfidence Probe(
      const core::ImportRequest& request) const override;

  [[nodiscard]] core::ImportResult Import(
      const core::ImportRequest& request) const override;

 private:
  const core::FormatDescriptor descriptor_;
};

}  // namespace edit_atlas::formats::cmx3600

#endif  // EDIT_ATLAS_FORMATS_CMX3600_CMX3600_IMPORTER_HPP_
