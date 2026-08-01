if(NOT DEFINED EDIT_ATLAS_BUNDLE)
    message(FATAL_ERROR "EDIT_ATLAS_BUNDLE is required.")
endif()

if(NOT IS_DIRECTORY "${EDIT_ATLAS_BUNDLE}")
    message(
        FATAL_ERROR
        "Edit Atlas application bundle does not exist: ${EDIT_ATLAS_BUNDLE}"
    )
endif()

set(
    edit_atlas_compatibility_dylibs
    libQt6Concurrent.6.dylib
    libQt6Core.6.dylib
    libQt6Gui.6.dylib
    libQt6Widgets.6.dylib
    libxlsxwriter.dylib
)
foreach(edit_atlas_compatibility_dylib IN LISTS edit_atlas_compatibility_dylibs)
    set(
        edit_atlas_compatibility_path
        "${EDIT_ATLAS_BUNDLE}/Contents/Frameworks/${edit_atlas_compatibility_dylib}"
    )
    if(
        EXISTS "${edit_atlas_compatibility_path}"
        AND NOT IS_SYMLINK "${edit_atlas_compatibility_path}"
    )
        message(
            FATAL_ERROR
            "Compatibility dylib is a duplicate regular file instead of a "
            "symlink: ${edit_atlas_compatibility_path}"
        )
    endif()
endforeach()

file(
    GLOB_RECURSE edit_atlas_bundle_files
    LIST_DIRECTORIES FALSE
    "${EDIT_ATLAS_BUNDLE}/*"
)

set(edit_atlas_mach_o_count 0)
foreach(edit_atlas_bundle_file IN LISTS edit_atlas_bundle_files)
    execute_process(
        COMMAND file -b "${edit_atlas_bundle_file}"
        RESULT_VARIABLE edit_atlas_file_result
        OUTPUT_VARIABLE edit_atlas_file_description
        ERROR_VARIABLE edit_atlas_file_error
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT edit_atlas_file_result EQUAL 0)
        message(
            FATAL_ERROR
            "Could not inspect ${edit_atlas_bundle_file}:\n"
            "${edit_atlas_file_error}"
        )
    endif()
    if(NOT edit_atlas_file_description MATCHES "Mach-O")
        continue()
    endif()

    math(EXPR edit_atlas_mach_o_count "${edit_atlas_mach_o_count} + 1")
    execute_process(
        COMMAND
            lipo
            "${edit_atlas_bundle_file}"
            -verify_arch
            x86_64
            arm64
        RESULT_VARIABLE edit_atlas_lipo_result
        ERROR_VARIABLE edit_atlas_lipo_error
    )
    if(NOT edit_atlas_lipo_result EQUAL 0)
        execute_process(
            COMMAND lipo "${edit_atlas_bundle_file}" -archs
            RESULT_VARIABLE edit_atlas_archs_result
            OUTPUT_VARIABLE edit_atlas_archs
            ERROR_VARIABLE edit_atlas_archs_error
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT edit_atlas_archs_result EQUAL 0)
            set(edit_atlas_archs "${edit_atlas_archs_error}")
        endif()
        message(
            FATAL_ERROR
            "Mach-O file is not universal x86_64 and ARM64: "
            "${edit_atlas_bundle_file}\n"
            "Architectures: ${edit_atlas_archs}\n"
            "${edit_atlas_lipo_error}"
        )
    endif()
endforeach()

if(edit_atlas_mach_o_count EQUAL 0)
    message(FATAL_ERROR "No Mach-O files were found under ${EDIT_ATLAS_BUNDLE}.")
endif()

message(
    STATUS
    "Verified ${edit_atlas_mach_o_count} universal Mach-O file(s) under "
    "${EDIT_ATLAS_BUNDLE}."
)
