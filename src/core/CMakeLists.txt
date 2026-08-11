add_library(edit_atlas_core STATIC)
add_library(EditAtlas::Core ALIAS edit_atlas_core)

set_target_properties(
    edit_atlas_core
    PROPERTIES
        EXPORT_NAME Core
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_core
    PRIVATE
        timeline_document_pipeline.cpp
        format_registry.cpp
        timecode.cpp
        timeline_projection.cpp
        version.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/edit_atlas/core/timeline_document_pipeline.hpp
            include/edit_atlas/core/editorial_timeline.hpp
            include/edit_atlas/core/format.hpp
            include/edit_atlas/core/format_registry.hpp
            include/edit_atlas/core/rgb_image.hpp
            include/edit_atlas/core/timecode.hpp
            include/edit_atlas/core/timeline_projection.hpp
            include/edit_atlas/core/version.hpp
)

target_compile_definitions(
    edit_atlas_core
    PRIVATE EDIT_ATLAS_VERSION="${PROJECT_VERSION}"
)

target_compile_features(edit_atlas_core PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_core)

install(
    TARGETS edit_atlas_core
    EXPORT EditAtlasTargets
    FILE_SET HEADERS
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT Development
    ARCHIVE
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT Development
    LIBRARY
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT Development
    RUNTIME
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Development
)

install(
    EXPORT EditAtlasTargets
    NAMESPACE EditAtlas::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/EditAtlas"
    COMPONENT Development
)
