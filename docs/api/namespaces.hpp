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

#ifndef EDIT_ATLAS_DOCS_API_NAMESPACES_HPP_
#define EDIT_ATLAS_DOCS_API_NAMESPACES_HPP_

/// Public C++ interfaces for Edit Atlas.
namespace edit_atlas {

/// Cross-platform command-line frontend.
namespace cli {}

/// Format-independent editorial domain types and in-memory pipelines.
namespace core {}

/// Built-in editorial interchange and report formats.
namespace formats {

/// CMX 3600 EDL import support.
namespace cmx3600 {}

/// Microsoft Excel workbook export support.
namespace xlsx {}

}  // namespace formats

/// UI-independent local-document application workflows.
namespace services {}

/// Persistent logging and privacy-limited diagnostic support.
namespace support {}

}  // namespace edit_atlas

#endif  // EDIT_ATLAS_DOCS_API_NAMESPACES_HPP_
