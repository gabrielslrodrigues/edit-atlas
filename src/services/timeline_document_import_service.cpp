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

#include "edit_atlas/services/timeline_document_import_service.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "edit_atlas/core/format.hpp"
#include "edit_atlas/core/format_registry.hpp"
#include "edit_atlas/storage/local_file.hpp"

namespace edit_atlas::services {
namespace {

[[nodiscard]] std::string Utf8Path(const std::filesystem::path& path) {
  const auto utf8 = path.generic_u8string();
  return std::string{reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

[[nodiscard]] TimelineDocumentImportFailure FileFailure(
    const storage::LocalFileFailure& failure) {
  const auto kind = failure.kind == storage::LocalFileFailureKind::kOpenFailed
                        ? TimelineDocumentImportFailureKind::kOpenFailed
                        : TimelineDocumentImportFailureKind::kReadFailed;
  return TimelineDocumentImportFailure{
      .path = failure.path,
      .kind = kind,
      .filesystem_error = failure.filesystem_error,
      .diagnostics = {},
  };
}

}  // namespace

TimelineDocumentImportService::TimelineDocumentImportService(
    const core::FormatRegistry& registry) noexcept
    : pipeline_(registry) {}

TimelineDocumentImportResult TimelineDocumentImportService::Import(
    TimelineDocumentImportRequest request) const {
  auto content = storage::ReadLocalFile(request.path);
  if (!content.has_value()) {
    return std::unexpected(FileFailure(content.error()));
  }

  auto extension = Utf8Path(request.path.extension());
  if (extension.starts_with('.')) {
    extension.erase(extension.begin());
  }
  const core::ImportRequest import_request{
      .content = *content,
      .source_name = Utf8Path(request.path),
      .extension = std::move(extension),
      .options = std::move(request.options),
  };
  const auto format_identifier =
      request.format_identifier.has_value()
          ? std::optional<std::string_view>{*request.format_identifier}
          : std::nullopt;
  auto import_result = pipeline_.Import(import_request, format_identifier);
  if (!import_result.document.has_value()) {
    return std::unexpected(TimelineDocumentImportFailure{
        .path = std::move(request.path),
        .kind = TimelineDocumentImportFailureKind::kImportFailed,
        .filesystem_error = {},
        .diagnostics = std::move(import_result.diagnostics),
    });
  }

  return TimelineDocumentImportReceipt{
      .path = std::move(request.path),
      .timeline = std::move(*import_result.document),
      .diagnostics = std::move(import_result.diagnostics),
  };
}

}  // namespace edit_atlas::services
