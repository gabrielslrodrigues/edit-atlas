set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_FIXUP_ELF_RPATH ON)

# Ports are built with the pinned compiler too. A port that links an optional
# dependency only under one compiler would otherwise give a local package a
# different runtime closure than the CI one.
get_filename_component(_edit_atlas_source_dir "${CMAKE_CURRENT_LIST_DIR}/.."
    ABSOLUTE)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE
    "${_edit_atlas_source_dir}/cmake/LinuxClangToolchain.cmake")

if(PORT STREQUAL "qtbase")
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
        -DCMAKE_DISABLE_FIND_PACKAGE_ATSPI2=OFF
    )
endif()
