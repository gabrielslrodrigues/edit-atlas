set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/EditAtlasPlatformSupport.cmake")
list(
    APPEND
    VCPKG_HASH_ADDITIONAL_FILES
    "${CMAKE_CURRENT_LIST_DIR}/../cmake/EditAtlasPlatformSupport.cmake"
)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES "x86_64;arm64")
set(
    VCPKG_OSX_DEPLOYMENT_TARGET
    "${EDIT_ATLAS_MACOS_MINIMUM_VERSION}"
)

# Autoconf has no canonical universal CPU name. The compiler still receives
# both architecture flags from VCPKG_OSX_ARCHITECTURES.
set(VCPKG_MAKE_BUILD_TRIPLET "--host=x86_64-apple-darwin")
