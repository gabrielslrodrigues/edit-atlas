#include <edit_atlas/core/format_registry.hpp>

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace edit_atlas::core {
namespace {

[[nodiscard]] bool IsLowerAsciiLetter(char character) noexcept {
    return character >= 'a' && character <= 'z';
}

[[nodiscard]] bool IsAsciiDigit(char character) noexcept {
    return character >= '0' && character <= '9';
}

[[nodiscard]] bool IsIdentifierCharacter(char character) noexcept {
    return IsLowerAsciiLetter(character) || IsAsciiDigit(character) ||
           character == '-' || character == '_' || character == '.';
}

[[nodiscard]] bool IsValidIdentifier(std::string_view identifier) noexcept {
    if (identifier.empty() ||
        !(IsLowerAsciiLetter(identifier.front()) ||
          IsAsciiDigit(identifier.front())) ||
        !(IsLowerAsciiLetter(identifier.back()) ||
          IsAsciiDigit(identifier.back()))) {
        return false;
    }

    return std::ranges::all_of(identifier, IsIdentifierCharacter);
}

[[nodiscard]] bool IsValidExtension(std::string_view extension) noexcept {
    return !extension.empty() &&
           std::ranges::all_of(extension, [](char character) {
               return IsLowerAsciiLetter(character) || IsAsciiDigit(character);
           });
}

[[nodiscard]] std::optional<FormatRegistrationError>
ValidateDescriptor(const FormatDescriptor &descriptor) {
    if (!IsValidIdentifier(descriptor.identifier)) {
        return FormatRegistrationError::kInvalidIdentifier;
    }
    if (descriptor.display_name.empty()) {
        return FormatRegistrationError::kEmptyDisplayName;
    }
    if (descriptor.extensions.empty()) {
        return FormatRegistrationError::kInvalidExtension;
    }

    std::vector<std::string_view> extensions;
    extensions.reserve(descriptor.extensions.size());
    for (const auto &extension : descriptor.extensions) {
        if (!IsValidExtension(extension) ||
            std::ranges::find(extensions, extension) != extensions.end()) {
            return FormatRegistrationError::kInvalidExtension;
        }
        extensions.emplace_back(extension);
    }

    return std::nullopt;
}

[[nodiscard]] char LowerAscii(char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character + ('a' - 'A'));
    }
    return character;
}

[[nodiscard]] std::string NormalizeExtension(std::string_view extension) {
    if (extension.starts_with('.')) {
        extension.remove_prefix(1);
    }

    std::string normalized;
    normalized.reserve(extension.size());
    std::ranges::transform(extension, std::back_inserter(normalized),
                           LowerAscii);
    return normalized;
}

template <typename Handler>
[[nodiscard]] bool SupportsExtension(const Handler &handler,
                                     std::string_view extension) {
    const auto normalized = NormalizeExtension(extension);
    return std::ranges::find(handler.descriptor().extensions, normalized) !=
           handler.descriptor().extensions.end();
}

} // namespace

std::expected<void, FormatRegistrationError>
FormatRegistry::RegisterImporter(std::unique_ptr<Importer> importer) {
    if (importer == nullptr) {
        return std::unexpected(FormatRegistrationError::kNullHandler);
    }
    if (const auto error = ValidateDescriptor(importer->descriptor())) {
        return std::unexpected(*error);
    }
    if (FindImporter(importer->descriptor().identifier) != nullptr) {
        return std::unexpected(FormatRegistrationError::kDuplicateImporter);
    }

    auto identifier = importer->descriptor().identifier;
    importers_.emplace(std::move(identifier), std::move(importer));
    return {};
}

std::expected<void, FormatRegistrationError>
FormatRegistry::RegisterExporter(std::unique_ptr<Exporter> exporter) {
    if (exporter == nullptr) {
        return std::unexpected(FormatRegistrationError::kNullHandler);
    }
    if (const auto error = ValidateDescriptor(exporter->descriptor())) {
        return std::unexpected(*error);
    }
    if (FindExporter(exporter->descriptor().identifier) != nullptr) {
        return std::unexpected(FormatRegistrationError::kDuplicateExporter);
    }

    auto identifier = exporter->descriptor().identifier;
    exporters_.emplace(std::move(identifier), std::move(exporter));
    return {};
}

std::vector<FormatDescriptor> FormatRegistry::importer_formats(void) const {
    std::vector<FormatDescriptor> formats;
    formats.reserve(importers_.size());
    std::ranges::transform(
        importers_, std::back_inserter(formats),
        [](const auto &entry) { return entry.second->descriptor(); });
    return formats;
}

std::vector<FormatDescriptor> FormatRegistry::exporter_formats(void) const {
    std::vector<FormatDescriptor> formats;
    formats.reserve(exporters_.size());
    std::ranges::transform(
        exporters_, std::back_inserter(formats),
        [](const auto &entry) { return entry.second->descriptor(); });
    return formats;
}

const Importer *
FormatRegistry::FindImporter(std::string_view identifier) const noexcept {
    const auto match = importers_.find(identifier);
    return match == importers_.end() ? nullptr : match->second.get();
}

const Exporter *
FormatRegistry::FindExporter(std::string_view identifier) const noexcept {
    const auto match = exporters_.find(identifier);
    return match == exporters_.end() ? nullptr : match->second.get();
}

std::vector<const Importer *>
FormatRegistry::ImportersForExtension(std::string_view extension) const {
    std::vector<const Importer *> matches;
    for (const auto &entry : importers_) {
        if (SupportsExtension(*entry.second, extension)) {
            matches.emplace_back(entry.second.get());
        }
    }
    return matches;
}

std::vector<const Exporter *>
FormatRegistry::ExportersForExtension(std::string_view extension) const {
    std::vector<const Exporter *> matches;
    for (const auto &entry : exporters_) {
        if (SupportsExtension(*entry.second, extension)) {
            matches.emplace_back(entry.second.get());
        }
    }
    return matches;
}

std::vector<const Importer *> FormatRegistry::importers(void) const {
    std::vector<const Importer *> result;
    result.reserve(importers_.size());
    std::ranges::transform(
        importers_, std::back_inserter(result),
        [](const auto &entry) { return entry.second.get(); });
    return result;
}

} // namespace edit_atlas::core
