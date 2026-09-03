# Pins the Linux compiler for the project and for every vcpkg port, so a
# local build and a CI build produce the same binaries and, more importantly,
# the same runtime dependency closure. The compiler is not cosmetic here:
# vcpkg's libb2 enables OpenMP when the compiler supports it, so a Clang build
# links libomp while a GCC build links nothing, and Qt's deployment tooling
# treats that library as a system one and leaves it neither bundled nor
# declared.
#
# Clang 19 is the floor. Ubuntu's Clang 18 reports `__cpp_concepts` as
# 201907, while libstdc++ gates `std::expected` on 202002, so `<expected>`
# expands to nothing and this project's C++23 code fails with "no template
# named 'expected'". Versioned names are preferred over the bare one so a
# distribution whose default Clang is too old still configures.
#
# Set EDIT_ATLAS_LINUX_C_COMPILER and EDIT_ATLAS_LINUX_CXX_COMPILER in the
# environment for a deliberate cross-compiler check. They are read from the
# environment rather than the cache so one setting reaches the project and the
# port builds, which are separate CMake invocations, and they bypass the
# search below.

set(_edit_atlas_minimum_clang 19)

# Reports the major version of a Clang executable, or an empty string.
function(_edit_atlas_clang_major executable output)
    execute_process(
        COMMAND "${executable}" -dumpversion
        OUTPUT_VARIABLE version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE status
    )
    if(NOT status EQUAL 0)
        set("${output}" "" PARENT_SCOPE)
        return()
    endif()
    string(REGEX MATCH "^[0-9]+" major "${version}")
    set("${output}" "${major}" PARENT_SCOPE)
endfunction()

# Selects a matched pair of Clang drivers, preferring versioned names. Both
# drivers must come from the same suffix: searching for them independently
# could pair a versioned C compiler with a too-old default C++ one.
function(_edit_atlas_select_clang c_output cxx_output)
    foreach(suffix IN ITEMS "-22" "-21" "-20" "-19" "")
        # NO_CACHE keeps each probe independent; a cached result would be
        # reused for the other driver and select the wrong one.
        find_program(c_candidate NAMES "clang${suffix}" NO_CACHE)
        find_program(cxx_candidate NAMES "clang++${suffix}" NO_CACHE)
        if(NOT c_candidate OR NOT cxx_candidate)
            continue()
        endif()
        _edit_atlas_clang_major("${c_candidate}" c_major)
        _edit_atlas_clang_major("${cxx_candidate}" cxx_major)
        if(NOT c_major OR NOT cxx_major OR NOT c_major EQUAL cxx_major)
            continue()
        endif()
        if(c_major GREATER_EQUAL _edit_atlas_minimum_clang)
            set("${c_output}" "${c_candidate}" PARENT_SCOPE)
            set("${cxx_output}" "${cxx_candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR
        "No matched Clang ${_edit_atlas_minimum_clang} or newer was found. "
        "Install one, or set EDIT_ATLAS_LINUX_C_COMPILER and "
        "EDIT_ATLAS_LINUX_CXX_COMPILER to a deliberate alternative. Clang 18 "
        "and older cannot compile this project's C++23 code against "
        "libstdc++, because they under-report __cpp_concepts and libstdc++ "
        "then hides std::expected."
    )
endfunction()

if(DEFINED ENV{EDIT_ATLAS_LINUX_C_COMPILER}
   AND DEFINED ENV{EDIT_ATLAS_LINUX_CXX_COMPILER})
    set(CMAKE_C_COMPILER "$ENV{EDIT_ATLAS_LINUX_C_COMPILER}")
    set(CMAKE_CXX_COMPILER "$ENV{EDIT_ATLAS_LINUX_CXX_COMPILER}")
elseif(DEFINED ENV{EDIT_ATLAS_LINUX_C_COMPILER}
       OR DEFINED ENV{EDIT_ATLAS_LINUX_CXX_COMPILER})
    message(FATAL_ERROR
        "Set both EDIT_ATLAS_LINUX_C_COMPILER and "
        "EDIT_ATLAS_LINUX_CXX_COMPILER, or neither: overriding one leaves "
        "the languages on different compilers."
    )
else()
    _edit_atlas_select_clang(_edit_atlas_c _edit_atlas_cxx)
    set(CMAKE_C_COMPILER "${_edit_atlas_c}")
    set(CMAKE_CXX_COMPILER "${_edit_atlas_cxx}")
endif()

# Chainloading replaces vcpkg's platform toolchain for a port, so include it
# rather than reimplementing the position-independent-code and flag plumbing
# ports rely on. It assigns compilers only when cross-compiling and only when
# they are undefined, so the choice above stands.
#
# The project build loads this same file through vcpkg.cmake, where that
# toolchain is neither loaded nor wanted: CMake's own platform handling
# applies there, and only the compiler needs pinning. A port build is what
# defines VCPKG_TARGET_ARCHITECTURE, which is also what the included file
# reads.
if(DEFINED VCPKG_TARGET_ARCHITECTURE)
    include(
        "${CMAKE_CURRENT_LIST_DIR}/../vcpkg/scripts/toolchains/linux.cmake"
    )
endif()
