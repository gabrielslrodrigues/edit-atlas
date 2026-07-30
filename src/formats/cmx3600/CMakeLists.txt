add_library(edit_atlas_cmx3600 STATIC)
add_library(EditAtlas::Cmx3600 ALIAS edit_atlas_cmx3600)

set_target_properties(
    edit_atlas_cmx3600
    PROPERTIES
        EXPORT_NAME Cmx3600
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_cmx3600
    PRIVATE
        cmx3600_importer.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/edit_atlas/formats/cmx3600/cmx3600_importer.hpp
)

target_link_libraries(edit_atlas_cmx3600 PUBLIC EditAtlas::Core)
target_compile_features(edit_atlas_cmx3600 PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_cmx3600)

install(
    TARGETS edit_atlas_cmx3600
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
