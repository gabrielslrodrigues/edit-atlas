add_library(edit_atlas_cli_frontend STATIC)
add_library(EditAtlas::CliFrontend ALIAS edit_atlas_cli_frontend)

find_package(CLI11 CONFIG REQUIRED)

set_target_properties(
    edit_atlas_cli_frontend
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_cli_frontend
    PRIVATE
        application.cpp
        diagnostic_output.cpp
        diagnostic_output.hpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/edit_atlas/cli/application.hpp
)

target_link_libraries(
    edit_atlas_cli_frontend
    PUBLIC
        EditAtlas::Services
    PRIVATE
        CLI11::CLI11
)

target_compile_features(edit_atlas_cli_frontend PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_cli_frontend)

add_executable(edit_atlas_cli main.cpp)
set_target_properties(edit_atlas_cli PROPERTIES OUTPUT_NAME "edit-atlas-cli")

target_link_libraries(edit_atlas_cli PRIVATE EditAtlas::CliFrontend)
target_compile_features(edit_atlas_cli PRIVATE cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_cli)

if(APPLE)
    set_target_properties(
        edit_atlas_cli
        PROPERTIES
            INSTALL_RPATH "@loader_path/../Frameworks"
    )
    set(edit_atlas_cli_install_directory
        "edit-atlas.app/Contents/MacOS"
    )
elseif(LINUX)
    set_target_properties(
        edit_atlas_cli
        PROPERTIES
            INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
    set(edit_atlas_cli_install_directory "${CMAKE_INSTALL_BINDIR}")
else()
    set(edit_atlas_cli_install_directory "${CMAKE_INSTALL_BINDIR}")
endif()

install(
    TARGETS edit_atlas_cli
    RUNTIME
        DESTINATION "${edit_atlas_cli_install_directory}"
        COMPONENT Runtime
)
