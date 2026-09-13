get_filename_component(edit_atlas_style_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(WIN32)
    set(edit_atlas_style_python "${edit_atlas_style_root}/tools/style/.venv/Scripts/python.exe")
else()
    set(edit_atlas_style_python "${edit_atlas_style_root}/tools/style/.venv/bin/python")
endif()
if(NOT EXISTS "${edit_atlas_style_python}")
    message(FATAL_ERROR "Style tools are missing. Run cmake -P cmake/BootstrapStyle.cmake first.")
endif()
execute_process(
    COMMAND "${edit_atlas_style_python}" "${edit_atlas_style_root}/tools/style/style.py"
        "${EDIT_ATLAS_STYLE_MODE}"
    WORKING_DIRECTORY "${edit_atlas_style_root}"
    COMMAND_ERROR_IS_FATAL ANY
)
