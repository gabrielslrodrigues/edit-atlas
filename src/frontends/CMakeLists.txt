include(EditAtlasApplication)

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

if(EDIT_ATLAS_BUILD_PACKAGING_APPLICATIONS)
    edit_atlas_define_packaged_frontend(quick edit_atlas_quick_package)
    edit_atlas_define_packaged_frontend(widgets edit_atlas_widgets_package)
endif()

edit_atlas_define_default_frontend("${EDIT_ATLAS_DEFAULT_FRONTEND}")
edit_atlas_install_shared_application_runtime()
edit_atlas_install_cli_application(edit_atlas_cli)
