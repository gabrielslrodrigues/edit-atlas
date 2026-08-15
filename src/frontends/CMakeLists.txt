include(EditAtlasApplication)

add_subdirectory(cli)
add_subdirectory(quick)
add_subdirectory(widgets)

set(edit_atlas_default_application edit_atlas_widgets)
edit_atlas_define_default_application("${edit_atlas_default_application}")
edit_atlas_install_cli_application(edit_atlas_cli)
