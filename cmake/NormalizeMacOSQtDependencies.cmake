if(NOT DEFINED EDIT_ATLAS_EXECUTABLE)
    message(FATAL_ERROR "EDIT_ATLAS_EXECUTABLE is required.")
endif()

find_program(edit_atlas_install_name_tool install_name_tool REQUIRED)
find_program(edit_atlas_otool otool REQUIRED)

execute_process(
    COMMAND "${edit_atlas_otool}" -L "${EDIT_ATLAS_EXECUTABLE}"
    OUTPUT_VARIABLE edit_atlas_dependencies
    ERROR_VARIABLE edit_atlas_otool_error
    RESULT_VARIABLE edit_atlas_otool_result
)
if(NOT edit_atlas_otool_result EQUAL 0)
    message(
        FATAL_ERROR
        "Could not inspect ${EDIT_ATLAS_EXECUTABLE}: "
        "${edit_atlas_otool_error}"
    )
endif()

string(
    REPLACE "\n" ";"
    edit_atlas_dependency_lines
    "${edit_atlas_dependencies}"
)
foreach(edit_atlas_dependency_line IN LISTS edit_atlas_dependency_lines)
    if(
        NOT edit_atlas_dependency_line
            MATCHES
                "^[ \t]*((@[^/]+/|/).*libQt6[^ \t]+\\.6\\.[0-9]+\\.[0-9]+\\.dylib)[ \t]+\\("
    )
        continue()
    endif()

    set(edit_atlas_versioned_dependency "${CMAKE_MATCH_1}")
    string(
        REGEX REPLACE
        "\\.6\\.[0-9]+\\.[0-9]+\\.dylib$"
        ".6.dylib"
        edit_atlas_abi_dependency
        "${edit_atlas_versioned_dependency}"
    )
    execute_process(
        COMMAND
            "${edit_atlas_install_name_tool}"
            -change
            "${edit_atlas_versioned_dependency}"
            "${edit_atlas_abi_dependency}"
            "${EDIT_ATLAS_EXECUTABLE}"
        ERROR_VARIABLE edit_atlas_install_name_error
        RESULT_VARIABLE edit_atlas_install_name_result
    )
    if(NOT edit_atlas_install_name_result EQUAL 0)
        message(
            FATAL_ERROR
            "Could not normalize ${edit_atlas_versioned_dependency}: "
            "${edit_atlas_install_name_error}"
        )
    endif()
endforeach()
