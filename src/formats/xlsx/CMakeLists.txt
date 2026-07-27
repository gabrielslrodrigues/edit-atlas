find_package(Libxlsxwriter REQUIRED)
find_package(minizip CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

add_library(edit_atlas_xlsx STATIC)
add_library(EditAtlas::Xlsx ALIAS edit_atlas_xlsx)

set_target_properties(
    edit_atlas_xlsx
    PROPERTIES
        EXPORT_NAME Xlsx
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_xlsx
    PRIVATE
        xlsx_exporter.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/edit_atlas/formats/xlsx/xlsx_exporter.hpp
)

target_link_libraries(
    edit_atlas_xlsx
    PUBLIC EditAtlas::Core
    PRIVATE
        Libxlsxwriter::Libxlsxwriter
        $<IF:$<TARGET_EXISTS:MINIZIP::minizip>,MINIZIP::minizip,MINIZIP::minizipstatic>
        ZLIB::ZLIB
)

target_compile_features(edit_atlas_xlsx PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_xlsx)

install(
    TARGETS edit_atlas_xlsx
    EXPORT EditAtlasTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
