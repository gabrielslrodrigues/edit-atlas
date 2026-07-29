#include <edit_atlas/app/document_loader.hpp>

#include <edit_atlas/core/document_pipeline.hpp>
#include <edit_atlas/core/editorial_timeline.hpp>
#include <edit_atlas/core/format.hpp>

#include <edit_atlas/formats/cmx3600/cmx3600_importer.hpp>

#include <QByteArray>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIODevice>
#include <QString>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace edit_atlas::app {

DocumentLoadResult LoadDocument(const core::FormatRegistry &registry,
                                const QString &path,
                                std::optional<std::string> frame_rate) {
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        return DocumentLoadResult{
            .path = path,
            .error = DocumentLoadError::kOpenFailed,
            .error_detail = file.errorString(),
            .import_result = {},
        };
    }

    const auto content = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return DocumentLoadResult{
            .path = path,
            .error = DocumentLoadError::kReadFailed,
            .error_detail = file.errorString(),
            .import_result = {},
        };
    }

    std::vector<core::MetadataEntry> options;
    if (frame_rate.has_value()) {
        options.emplace_back(core::MetadataEntry{
            .key = std::string{formats::cmx3600::kFrameRateOption},
            .value = std::move(*frame_rate),
        });
    }

    const auto content_size = static_cast<std::size_t>(content.size());
    const auto bytes = std::span<const std::byte>{
        reinterpret_cast<const std::byte *>(content.constData()), content_size};
    const auto file_info = QFileInfo{path};
    const core::ImportRequest request{
        .content = bytes,
        .source_name = path.toStdString(),
        .extension = file_info.suffix().toStdString(),
        .options = std::move(options),
    };
    const core::DocumentPipeline pipeline{registry};
    return DocumentLoadResult{
        .path = path,
        .error = DocumentLoadError::kNone,
        .error_detail = {},
        .import_result = pipeline.Import(request),
    };
}

} // namespace edit_atlas::app
