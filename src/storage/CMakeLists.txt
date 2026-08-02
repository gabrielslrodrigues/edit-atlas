add_library(edit_atlas_storage STATIC)
add_library(EditAtlas::Storage ALIAS edit_atlas_storage)

set_target_properties(
    edit_atlas_storage
    PROPERTIES
        EXPORT_NAME Storage
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_storage
    PRIVATE
        local_file.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/edit_atlas/storage/local_file.hpp
)

target_compile_features(edit_atlas_storage PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_storage)

install(
    TARGETS edit_atlas_storage
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
