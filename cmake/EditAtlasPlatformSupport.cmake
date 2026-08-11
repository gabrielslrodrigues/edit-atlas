set(EDIT_ATLAS_MACOS_MINIMUM_VERSION "13.3")

function(edit_atlas_configure_apple_libcxx)
    if(NOT APPLE)
        return()
    endif()

    include(CheckCXXSourceCompiles)
    include(CMakePushCheckState)

    set(edit_atlas_stop_token_check [[
        #include <stop_token>

        int main(void) {
            std::stop_source source;
            return source.stop_possible() ? 0 : 1;
        }
    ]])

    check_cxx_source_compiles(
        "${edit_atlas_stop_token_check}"
        EDIT_ATLAS_APPLE_LIBCXX_HAS_STOP_TOKEN
    )
    if(EDIT_ATLAS_APPLE_LIBCXX_HAS_STOP_TOKEN)
        return()
    endif()

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "-fexperimental-library")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fexperimental-library")
    check_cxx_source_compiles(
        "${edit_atlas_stop_token_check}"
        EDIT_ATLAS_APPLE_LIBCXX_HAS_EXPERIMENTAL_STOP_TOKEN
    )
    cmake_pop_check_state()

    if(NOT EDIT_ATLAS_APPLE_LIBCXX_HAS_EXPERIMENTAL_STOP_TOKEN)
        message(
            FATAL_ERROR
            "The selected Apple libc++ does not provide std::stop_token "
            "with or without -fexperimental-library."
        )
    endif()

    message(
        STATUS
        "Enabling Apple libc++ experimental-library support for "
        "std::stop_token"
    )
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>"
    )
    add_link_options(
        "$<$<LINK_LANGUAGE:CXX>:-fexperimental-library>"
    )
endfunction()
