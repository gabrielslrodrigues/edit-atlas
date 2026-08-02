find_package(minizip CONFIG REQUIRED)

add_library(edit_atlas_support STATIC)
add_library(EditAtlas::Support ALIAS edit_atlas_support)

set_target_properties(
    edit_atlas_support
    PROPERTIES
        EXPORT_NAME Support
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_support
    PRIVATE
        application_logging.cpp
        support_bundle.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/edit_atlas/support/application_logging.hpp
            include/edit_atlas/support/support_bundle.hpp
)

target_link_libraries(
    edit_atlas_support
    PUBLIC
        EditAtlas::Storage
    PRIVATE
        spdlog::spdlog
        $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>
)

target_compile_features(edit_atlas_support PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_support)

install(
    TARGETS edit_atlas_support
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
