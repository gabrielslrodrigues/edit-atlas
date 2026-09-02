# Pins the Linux compiler for the project and for every vcpkg port, so a
# local build and a CI build produce the same binaries and, more importantly,
# the same runtime dependency closure. The compiler is not cosmetic here:
# vcpkg's libb2 enables OpenMP when the compiler supports it, so a Clang build
# links libomp while a GCC build links nothing, and Qt's deployment tooling
# treats that library as a system one and leaves it neither bundled nor
# declared.
#
# Set EDIT_ATLAS_LINUX_C_COMPILER and EDIT_ATLAS_LINUX_CXX_COMPILER in the
# environment for a deliberate cross-compiler check. They are read from the
# environment rather than the cache so one setting reaches the project and the
# port builds, which are separate CMake invocations.

if(DEFINED ENV{EDIT_ATLAS_LINUX_C_COMPILER})
    set(CMAKE_C_COMPILER "$ENV{EDIT_ATLAS_LINUX_C_COMPILER}")
else()
    set(CMAKE_C_COMPILER "clang")
endif()

if(DEFINED ENV{EDIT_ATLAS_LINUX_CXX_COMPILER})
    set(CMAKE_CXX_COMPILER "$ENV{EDIT_ATLAS_LINUX_CXX_COMPILER}")
else()
    set(CMAKE_CXX_COMPILER "clang++")
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
