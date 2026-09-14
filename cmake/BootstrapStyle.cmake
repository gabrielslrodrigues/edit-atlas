file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/MinimumVersion.txt"
    EDIT_ATLAS_MINIMUM_CMAKE_VERSION)
cmake_minimum_required(VERSION ${EDIT_ATLAS_MINIMUM_CMAKE_VERSION})
get_filename_component(edit_atlas_style_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
find_program(edit_atlas_style_uv uv REQUIRED)
execute_process(
    COMMAND "${edit_atlas_style_uv}" --version
    OUTPUT_VARIABLE edit_atlas_style_uv_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
if(NOT edit_atlas_style_uv_version MATCHES "^uv (0\\.12\\.[0-9]+)( |$)")
    message(FATAL_ERROR "Style bootstrap requires uv >=0.12.3,<0.13.")
endif()
if(CMAKE_MATCH_1 VERSION_LESS "0.12.3")
    message(FATAL_ERROR "Style bootstrap requires uv >=0.12.3,<0.13.")
endif()
execute_process(
    COMMAND "${edit_atlas_style_uv}" venv --python 3.12 tools/style/.venv
    WORKING_DIRECTORY "${edit_atlas_style_root}"
    COMMAND_ERROR_IS_FATAL ANY
)
if(WIN32)
    set(edit_atlas_style_python "${edit_atlas_style_root}/tools/style/.venv/Scripts/python.exe")
else()
    set(edit_atlas_style_python "${edit_atlas_style_root}/tools/style/.venv/bin/python")
endif()
execute_process(
    COMMAND "${edit_atlas_style_uv}" pip sync --python "${edit_atlas_style_python}"
        "${edit_atlas_style_root}/tools/style/requirements.txt"
    WORKING_DIRECTORY "${edit_atlas_style_root}"
    COMMAND_ERROR_IS_FATAL ANY
)
if(WIN32)
    find_program(edit_atlas_style_pwsh pwsh REQUIRED)
    execute_process(
        COMMAND "${edit_atlas_style_pwsh}" -NoProfile -File
            "${edit_atlas_style_root}/tools/style/Install-StyleModule.ps1"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()
