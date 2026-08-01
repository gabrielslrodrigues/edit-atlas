#ifndef EDIT_ATLAS_DOCUMENTATION_NAMESPACES_HPP_
#define EDIT_ATLAS_DOCUMENTATION_NAMESPACES_HPP_

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

} // namespace formats

/// UI-independent local-document application workflows.
namespace services {}

/// Persistent logging and privacy-limited diagnostic support.
namespace support {}

} // namespace edit_atlas

#endif // EDIT_ATLAS_DOCUMENTATION_NAMESPACES_HPP_
