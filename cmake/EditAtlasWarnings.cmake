include_guard(GLOBAL)

function(edit_atlas_set_project_warnings target)
    if(MSVC)
        target_compile_options(
            "${target}"
            PRIVATE
                /W4
                /permissive-
                /utf-8
                /Zc:__cplusplus
        )

        if(EDIT_ATLAS_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            "${target}"
            PRIVATE
                -Wall
                -Wconversion
                -Wextra
                -Wpedantic
                -Wsign-conversion
        )

        if(EDIT_ATLAS_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE -Werror)
        endif()
    else()
        message(
            WARNING
            "Edit Atlas has no project warning policy for "
            "${CMAKE_CXX_COMPILER_ID}"
        )
    endif()
endfunction()
