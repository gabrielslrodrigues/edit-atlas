# std::expected is the only C++23 facility these sources use, so it decides
# which compilers work. CMAKE_CXX_STANDARD 23 only makes CMake pass
# -std=c++23; it does not establish that the standard library behind the
# compiler implements what the sources include, which is how a missing
# <expected> surfaced as "no template named 'expected'" mid-build.
#
# Clang 19: libstdc++ gates <expected> on __cpp_concepts >= 202002, which
# Clang 18 and older report as 201907.
# MSVC 19.33: the STL added <expected> in Visual Studio 2022 17.3.
# Apple Clang: no number. libc++ 16 was the first release with <expected>,
# but Apple's libc++ does not track upstream releases, so a version here
# would be a guess; the check below is what the numbers stand for anyway.

set(EDIT_ATLAS_MINIMUM_CLANG 19)
set(EDIT_ATLAS_MINIMUM_MSVC 19.33)

set(edit_atlas_compiler_support_directory "${CMAKE_CURRENT_LIST_DIR}")

# Reports the declared floor for a compiler family, or an empty string when
# the family has none.
function(edit_atlas_minimum_compiler_version family output)
    if(family STREQUAL "Clang")
        set("${output}" "${EDIT_ATLAS_MINIMUM_CLANG}" PARENT_SCOPE)
    elseif(family STREQUAL "MSVC")
        set("${output}" "${EDIT_ATLAS_MINIMUM_MSVC}" PARENT_SCOPE)
    else()
        set("${output}" "" PARENT_SCOPE)
    endif()
endfunction()

# Reports why a compiler is too old, or an empty string when it is acceptable
# or its family declares no floor.
function(edit_atlas_compiler_floor_failure family version output)
    edit_atlas_minimum_compiler_version("${family}" minimum)
    if(minimum STREQUAL "" OR NOT version VERSION_LESS "${minimum}")
        set("${output}" "" PARENT_SCOPE)
        return()
    endif()
    string(
        CONCAT
        failure
        "${family} ${version} does not provide std::expected, which this "
        "project's sources require. Use ${family} ${minimum} or newer."
    )
    set("${output}" "${failure}" PARENT_SCOPE)
endfunction()

# The Linux toolchain repeats the Clang floor because it has to choose a
# driver before CMake knows the compiler. It cannot read this file: vcpkg
# chainloads it and hashes its contents into every x64-linux port's ABI, so
# adding an include there would rebuild every port to share one integer. This
# is what keeps the two numbers equal instead.
function(edit_atlas_assert_linux_toolchain_floor)
    set(toolchain
        "${edit_atlas_compiler_support_directory}/LinuxClangToolchain.cmake"
    )
    if(NOT EXISTS "${toolchain}")
        message(FATAL_ERROR "The Linux toolchain is missing: ${toolchain}")
    endif()

    file(
        STRINGS "${toolchain}" declaration
        REGEX "^set\\(_edit_atlas_minimum_clang [0-9]+\\)$"
    )
    if(NOT declaration MATCHES "([0-9]+)")
        message(
            FATAL_ERROR
            "${toolchain} declares no Clang floor. It has to select a driver "
            "before CMake knows the compiler, so it holds its own copy of "
            "the number this file declares."
        )
    endif()

    if(NOT CMAKE_MATCH_1 STREQUAL "${EDIT_ATLAS_MINIMUM_CLANG}")
        message(
            FATAL_ERROR
            "The Clang floor disagrees with itself: ${toolchain} selects "
            "${CMAKE_MATCH_1} and this file declares "
            "${EDIT_ATLAS_MINIMUM_CLANG}. Change both."
        )
    endif()
endfunction()

# Fails configuration when the selected compiler cannot deliver the C++23
# library facilities the sources use.
function(edit_atlas_require_supported_compiler)
    edit_atlas_assert_linux_toolchain_floor()

    edit_atlas_compiler_floor_failure(
        "${CMAKE_CXX_COMPILER_ID}"
        "${CMAKE_CXX_COMPILER_VERSION}"
        too_old
    )
    if(NOT too_old STREQUAL "")
        message(FATAL_ERROR "${too_old}")
    endif()

    # What the floors stand for, checked against the toolchain in hand rather
    # than inferred from its version. This is the only guard on macOS, and it
    # also covers a new enough compiler paired with an old standard library.
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles(
        [[
        #include <expected>

        int main(void) {
            const std::expected<int, int> value{1};
            return value.has_value() ? 0 : 1;
        }
        ]]
        EDIT_ATLAS_COMPILER_HAS_STD_EXPECTED
    )
    if(NOT EDIT_ATLAS_COMPILER_HAS_STD_EXPECTED)
        message(
            FATAL_ERROR
            "The standard library behind ${CMAKE_CXX_COMPILER_ID} "
            "${CMAKE_CXX_COMPILER_VERSION} does not provide std::expected, "
            "which this project's sources require. libstdc++ 12, libc++ 16, "
            "and the Visual Studio 2022 17.3 STL are the first releases that "
            "do."
        )
    endif()
endfunction()
