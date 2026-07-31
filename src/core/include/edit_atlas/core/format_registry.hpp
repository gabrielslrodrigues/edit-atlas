#ifndef EDIT_ATLAS_CORE_FORMAT_REGISTRY_HPP_
#define EDIT_ATLAS_CORE_FORMAT_REGISTRY_HPP_

#include <edit_atlas/core/format.hpp>

#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace edit_atlas::core {

/// Identifies why a format handler could not be registered.
enum class FormatRegistrationError {
    /// The supplied handler pointer is empty.
    kNullHandler,
    /// The format identifier is empty or not lowercase ASCII.
    kInvalidIdentifier,
    /// The human-readable format name is empty.
    kEmptyDisplayName,
    /// An advertised extension is invalid.
    kInvalidExtension,
    /// An importer already owns the format identifier.
    kDuplicateImporter,
    /// An exporter already owns the format identifier.
    kDuplicateExporter,
};

/// Owns format handlers and exposes their capabilities for discovery.
///
/// Importers and exporters use independent identifier namespaces, allowing one
/// format identifier to have both an importer and an exporter.
class FormatRegistry final {
  public:
    /// Creates an empty registry.
    FormatRegistry(void) = default;
    /// Destroys every registered handler.
    ~FormatRegistry(void) = default;

    /// Registries are non-copyable because they own handlers.
    FormatRegistry(const FormatRegistry &) = delete;
    /// Registries are non-copy-assignable because they own handlers.
    FormatRegistry &operator=(const FormatRegistry &) = delete;
    /// Transfers ownership of every registered handler.
    FormatRegistry(FormatRegistry &&) noexcept = default;
    /// Replaces the registry by moving every owned handler.
    FormatRegistry &operator=(FormatRegistry &&) noexcept = default;

    /// Consumes an importer and registers it when its descriptor is valid.
    [[nodiscard]] std::expected<void, FormatRegistrationError>
    RegisterImporter(std::unique_ptr<Importer> importer);

    /// Consumes an exporter and registers it when its descriptor is valid.
    [[nodiscard]] std::expected<void, FormatRegistrationError>
    RegisterExporter(std::unique_ptr<Exporter> exporter);

    /// Returns copies of importer descriptors in unspecified order.
    [[nodiscard]] std::vector<FormatDescriptor> importer_formats(void) const;

    /// Returns copies of exporter descriptors in unspecified order.
    [[nodiscard]] std::vector<FormatDescriptor> exporter_formats(void) const;

    /// Finds an importer by its case-sensitive stable identifier.
    [[nodiscard]] const Importer *
    FindImporter(std::string_view identifier) const noexcept;

    /// Finds an exporter by its case-sensitive stable identifier.
    [[nodiscard]] const Exporter *
    FindExporter(std::string_view identifier) const noexcept;

    /// Finds importers advertising \p extension.
    ///
    /// Matching is ASCII case-insensitive and accepts an optional leading dot.
    [[nodiscard]] std::vector<const Importer *>
    ImportersForExtension(std::string_view extension) const;

    /// Finds exporters advertising \p extension.
    ///
    /// Matching is ASCII case-insensitive and accepts an optional leading dot.
    [[nodiscard]] std::vector<const Exporter *>
    ExportersForExtension(std::string_view extension) const;

    /// Returns all registered importers in unspecified order.
    [[nodiscard]] std::vector<const Importer *> importers(void) const;

  private:
    std::map<std::string, std::unique_ptr<Importer>, std::less<>> importers_;
    std::map<std::string, std::unique_ptr<Exporter>, std::less<>> exporters_;
};

} // namespace edit_atlas::core

#endif // EDIT_ATLAS_CORE_FORMAT_REGISTRY_HPP_
