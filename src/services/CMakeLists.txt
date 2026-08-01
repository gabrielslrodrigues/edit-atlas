add_library(edit_atlas_services STATIC)
add_library(EditAtlas::Services ALIAS edit_atlas_services)

find_package(re2 CONFIG REQUIRED)

set_target_properties(
    edit_atlas_services
    PROPERTIES
        EXPORT_NAME Services
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_services
    PRIVATE
        built_in_formats.cpp
        document_export_service.cpp
        document_import_service.cpp
        timeline_filter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/edit_atlas/services/built_in_formats.hpp
            include/edit_atlas/services/document_export_service.hpp
            include/edit_atlas/services/document_import_service.hpp
            include/edit_atlas/services/timeline_filter.hpp
)

target_link_libraries(
    edit_atlas_services
    PUBLIC
        EditAtlas::Cmx3600
        EditAtlas::Core
        EditAtlas::Xlsx
    PRIVATE
        re2::re2
)

if(APPLE)
    install(
        FILES "$<TARGET_FILE:re2::re2>"
        DESTINATION "edit-atlas.app/Contents/Frameworks"
        COMPONENT Runtime
    )
endif()

target_compile_features(edit_atlas_services PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_services)

install(
    TARGETS edit_atlas_services
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
