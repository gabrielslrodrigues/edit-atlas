include(EditAtlasDefaultFrontend)

add_subdirectory(cli)

if(EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "all")
    add_subdirectory(quick)
    add_subdirectory(widgets)
elseif(EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "quick")
    add_subdirectory(quick)
    add_subdirectory(widgets EXCLUDE_FROM_ALL)
elseif(EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "widgets")
    add_subdirectory(quick EXCLUDE_FROM_ALL)
    add_subdirectory(widgets)
endif()

edit_atlas_define_default_frontend("${EDIT_ATLAS_DEFAULT_FRONTEND}")
edit_atlas_install_cli_application(edit_atlas_cli)
