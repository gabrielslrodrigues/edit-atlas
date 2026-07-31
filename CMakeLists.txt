cmake_minimum_required(VERSION 4.0)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/EditAtlasPlatformSupport.cmake")
if(CMAKE_HOST_APPLE)
    set(
        CMAKE_OSX_DEPLOYMENT_TARGET
        "${EDIT_ATLAS_MACOS_MINIMUM_VERSION}"
        CACHE STRING
        "Minimum supported macOS version"
        FORCE
    )
endif()

project(
    EditAtlas
    VERSION 0.1.0
    DESCRIPTION "Inspect editorial timelines and export structured reports"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(
    EDIT_ATLAS_BUILD_DOCUMENTATION
    "Generate public C++ API documentation"
    OFF
)
option(
    EDIT_ATLAS_DOCUMENTATION_ONLY
    "Configure only the documentation target"
    OFF
)

if(EDIT_ATLAS_DOCUMENTATION_ONLY AND NOT EDIT_ATLAS_BUILD_DOCUMENTATION)
    message(
        FATAL_ERROR
        "EDIT_ATLAS_DOCUMENTATION_ONLY requires "
        "EDIT_ATLAS_BUILD_DOCUMENTATION=ON."
    )
endif()

if(EDIT_ATLAS_BUILD_DOCUMENTATION)
    add_subdirectory(docs)
endif()

if(EDIT_ATLAS_DOCUMENTATION_ONLY)
    return()
endif()

include(GNUInstallDirs)
include(CTest)

option(
    EDIT_ATLAS_WARNINGS_AS_ERRORS
    "Treat compiler warnings from Edit Atlas targets as errors"
    OFF
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EditAtlasWarnings)
include(EditAtlasQtLinkage)

find_package(Qt6 REQUIRED COMPONENTS Concurrent LinguistTools Widgets)
find_package(spdlog CONFIG REQUIRED)

edit_atlas_require_dynamic_qt()

set(edit_atlas_vcpkg_share_directory
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share"
)
set(edit_atlas_qtbase_copyright
    "${edit_atlas_vcpkg_share_directory}/qtbase/copyright"
)
if(NOT EXISTS "${edit_atlas_qtbase_copyright}")
    message(
        FATAL_ERROR
        "The resolved Qt Base copyright file was not found at "
        "${edit_atlas_qtbase_copyright}."
    )
endif()

add_subdirectory(src/core)
add_subdirectory(src/formats/cmx3600)
add_subdirectory(src/formats/xlsx)
add_subdirectory(src/services)
add_subdirectory(src/support)
add_subdirectory(src/app)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()

if(APPLE)
    set(
        edit_atlas_license_install_dir
        "edit-atlas.app/Contents/Resources/licenses"
    )
else()
    set(
        edit_atlas_license_install_dir
        "${CMAKE_INSTALL_DATADIR}/licenses/edit-atlas"
    )
endif()

install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION "${edit_atlas_license_install_dir}"
    COMPONENT Runtime
)

file(
    GLOB edit_atlas_vcpkg_copyrights
    LIST_DIRECTORIES FALSE
    "${edit_atlas_vcpkg_share_directory}/*/copyright"
)
foreach(edit_atlas_vcpkg_copyright IN LISTS edit_atlas_vcpkg_copyrights)
    get_filename_component(
        edit_atlas_vcpkg_package_directory
        "${edit_atlas_vcpkg_copyright}"
        DIRECTORY
    )
    get_filename_component(
        edit_atlas_vcpkg_package
        "${edit_atlas_vcpkg_package_directory}"
        NAME
    )
    install(
        FILES "${edit_atlas_vcpkg_copyright}"
        DESTINATION "${edit_atlas_license_install_dir}/vcpkg"
        RENAME "${edit_atlas_vcpkg_package}-copyright"
        COMPONENT Runtime
    )
endforeach()

include(EditAtlasPackaging)
