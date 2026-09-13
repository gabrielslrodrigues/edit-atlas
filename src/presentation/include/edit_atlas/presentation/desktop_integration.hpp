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

#ifndef EDIT_ATLAS_PRESENTATION_DESKTOP_INTEGRATION_HPP_
#define EDIT_ATLAS_PRESENTATION_DESKTOP_INTEGRATION_HPP_

#include "QString"
#include "QUrl"

namespace edit_atlas::presentation::desktop_integration {

/// Opens the containing directory and selects the file when supported.
[[nodiscard]] bool RevealFile(const QString& path);

/// Opens a local directory in the platform file manager.
[[nodiscard]] bool OpenDirectory(const QString& path);

/// Opens an HTTP or HTTPS URL using the platform default application.
[[nodiscard]] bool OpenExternalUrl(const QUrl& url);

}  // namespace edit_atlas::presentation::desktop_integration

#endif  // EDIT_ATLAS_PRESENTATION_DESKTOP_INTEGRATION_HPP_
