if(NOT DEFINED EDIT_ATLAS_CLI_EXECUTABLE)
    message(FATAL_ERROR "EDIT_ATLAS_CLI_EXECUTABLE is required.")
endif()
if(NOT DEFINED EDIT_ATLAS_CLI_FIXTURE)
    message(FATAL_ERROR "EDIT_ATLAS_CLI_FIXTURE is required.")
endif()
if(NOT DEFINED EDIT_ATLAS_CLI_OUTPUT)
    message(FATAL_ERROR "EDIT_ATLAS_CLI_OUTPUT is required.")
endif()

file(REMOVE "${EDIT_ATLAS_CLI_OUTPUT}")
execute_process(
    COMMAND
        "${EDIT_ATLAS_CLI_EXECUTABLE}"
        convert
        --fps
        24
        "${EDIT_ATLAS_CLI_FIXTURE}"
        "${EDIT_ATLAS_CLI_OUTPUT}"
    RESULT_VARIABLE edit_atlas_cli_result
    OUTPUT_VARIABLE edit_atlas_cli_output
    ERROR_VARIABLE edit_atlas_cli_error
    ENCODING UTF-8
    TIMEOUT 15
)

if(NOT edit_atlas_cli_result EQUAL 0)
    message(
        FATAL_ERROR
        "CLI conversion failed with ${edit_atlas_cli_result}.\n"
        "stdout:\n${edit_atlas_cli_output}\n"
        "stderr:\n${edit_atlas_cli_error}"
    )
endif()
if(NOT edit_atlas_cli_error STREQUAL "")
    message(FATAL_ERROR "CLI wrote unexpected stderr: ${edit_atlas_cli_error}")
endif()
if(NOT EXISTS "${EDIT_ATLAS_CLI_OUTPUT}")
    message(FATAL_ERROR "CLI did not create ${EDIT_ATLAS_CLI_OUTPUT}.")
endif()

file(READ "${EDIT_ATLAS_CLI_OUTPUT}" edit_atlas_cli_signature LIMIT 2 HEX)
if(NOT edit_atlas_cli_signature STREQUAL "504b")
    message(
        FATAL_ERROR
        "CLI output is not an XLSX ZIP archive: ${edit_atlas_cli_signature}"
    )
endif()

file(REMOVE "${EDIT_ATLAS_CLI_OUTPUT}")
