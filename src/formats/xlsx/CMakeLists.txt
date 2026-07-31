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
        xlsx_workbook_text.cpp
        include/edit_atlas/formats/xlsx/detail/xlsx_workbook_text.hpp
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

if(APPLE)
    if(TARGET MINIZIP::minizip)
        set(edit_atlas_minizip_target MINIZIP::minizip)
    else()
        set(edit_atlas_minizip_target MINIZIP::minizipstatic)
    endif()

    install(
        FILES
            "$<TARGET_FILE:Libxlsxwriter::Libxlsxwriter>"
            "$<TARGET_FILE:${edit_atlas_minizip_target}>"
        DESTINATION "edit-atlas.app/Contents/Frameworks"
        COMPONENT Runtime
    )
endif()
