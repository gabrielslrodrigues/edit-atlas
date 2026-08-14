#ifndef EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_TEXT_HPP_
#define EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_TEXT_HPP_

#include <edit_atlas/core/editorial_timeline.hpp>

#include <QString>

#include <vector>

namespace edit_atlas::presentation::diagnostic_text {

/// Returns a localized presentation message for a diagnostic.
[[nodiscard]] QString Message(const core::Diagnostic &diagnostic);

/// Returns a localized, human-readable diagnostic list.
[[nodiscard]] QString Summary(const std::vector<core::Diagnostic> &diagnostics);

} // namespace edit_atlas::presentation::diagnostic_text

#endif // EDIT_ATLAS_PRESENTATION_DIAGNOSTIC_TEXT_HPP_
