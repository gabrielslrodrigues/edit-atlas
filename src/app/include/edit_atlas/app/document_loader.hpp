#ifndef EDIT_ATLAS_APP_DOCUMENT_LOADER_HPP_
#define EDIT_ATLAS_APP_DOCUMENT_LOADER_HPP_

#include <edit_atlas/core/format.hpp>
#include <edit_atlas/core/format_registry.hpp>

#include <QString>

#include <optional>
#include <string>

namespace edit_atlas::app {

/// Identifies a filesystem failure that occurred before format import.
enum class DocumentLoadError {
    kNone,
    kOpenFailed,
    kReadFailed,
};

/// The filesystem and importer result for one document-opening request.
struct DocumentLoadResult final {
    QString path;
    DocumentLoadError error;
    QString error_detail;
    core::ImportResult import_result;
};

/// Reads and imports a local document without interacting with the UI.
[[nodiscard]] DocumentLoadResult
LoadDocument(const core::FormatRegistry &registry, const QString &path,
             std::optional<std::string> frame_rate = std::nullopt);

} // namespace edit_atlas::app

#endif // EDIT_ATLAS_APP_DOCUMENT_LOADER_HPP_
