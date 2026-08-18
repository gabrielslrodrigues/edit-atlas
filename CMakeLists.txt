cmake_minimum_required(VERSION 4.0)

file(READ "${CMAKE_CURRENT_LIST_DIR}/vcpkg.json" edit_atlas_vcpkg_manifest)
string(
    JSON edit_atlas_version
    GET "${edit_atlas_vcpkg_manifest}"
    version-string
)

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
    VERSION "${edit_atlas_version}"
    DESCRIPTION "Inspect editorial timelines and export structured reports"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

edit_atlas_configure_apple_libcxx()

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

set(edit_atlas_runtime_install_libdir "${CMAKE_INSTALL_LIBDIR}")
if(LINUX)
    string(APPEND edit_atlas_runtime_install_libdir "/edit-atlas")
endif()

option(
    EDIT_ATLAS_WARNINGS_AS_ERRORS
    "Treat compiler warnings from Edit Atlas targets as errors"
    OFF
)

set(
    EDIT_ATLAS_DEFAULT_FRONTEND
    "widgets"
    CACHE STRING
    "Graphical frontend installed as the Edit Atlas application"
)
set_property(
    CACHE EDIT_ATLAS_DEFAULT_FRONTEND
    PROPERTY STRINGS quick widgets
)

set(
    EDIT_ATLAS_BUILD_FRONTENDS
    "all"
    CACHE STRING
    "Graphical frontends included in the default build"
)
set_property(
    CACHE EDIT_ATLAS_BUILD_FRONTENDS
    PROPERTY STRINGS all quick widgets
)

option(
    EDIT_ATLAS_BUILD_PACKAGING_APPLICATIONS
    "Build isolated frontend application targets used by packaging"
    OFF
)

if(
    NOT EDIT_ATLAS_DEFAULT_FRONTEND STREQUAL "quick"
    AND NOT EDIT_ATLAS_DEFAULT_FRONTEND STREQUAL "widgets"
)
    message(
        FATAL_ERROR
        "Unsupported Edit Atlas frontend: ${EDIT_ATLAS_DEFAULT_FRONTEND}. "
        "Expected quick or widgets."
    )
endif()

if(
    NOT EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "all"
    AND NOT EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "quick"
    AND NOT EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "widgets"
)
    message(
        FATAL_ERROR
        "Unsupported Edit Atlas frontend build selection: "
        "${EDIT_ATLAS_BUILD_FRONTENDS}. Expected all, quick, or widgets."
    )
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EditAtlasWarnings)
include(EditAtlasFfmpegLinkage)
include(EditAtlasQtLinkage)

find_package(
    Qt6
    REQUIRED
    COMPONENTS
        Concurrent
        Core
        Gui
        LinguistTools
        Qml
        Quick
        QuickControls2
        Widgets
)
find_package(FFMPEG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

edit_atlas_require_dynamic_qt()
edit_atlas_require_dynamic_ffmpeg()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    get_target_property(
        edit_atlas_qt_disabled_features
        Qt6::Gui
        QT_DISABLED_PUBLIC_FEATURES
    )
    if("accessibility_atspi_bridge" IN_LIST edit_atlas_qt_disabled_features)
        message(
            FATAL_ERROR
            "The Linux Qt build does not provide the AT-SPI accessibility "
            "bridge required by packaged E2E tests."
        )
    endif()
endif()

string(
    JSON edit_atlas_vcpkg_baseline
    GET "${edit_atlas_vcpkg_manifest}"
    builtin-baseline
)
set(EDIT_ATLAS_VERSION "${PROJECT_VERSION}")
set(EDIT_ATLAS_QT_VERSION "${Qt6_VERSION}")
set(EDIT_ATLAS_VCPKG_BASELINE "${edit_atlas_vcpkg_baseline}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/qt-source-offer.md.in"
    "${PROJECT_BINARY_DIR}/QT_SOURCE_OFFER.md"
    @ONLY
)
set(EDIT_ATLAS_FFMPEG_VERSION "${FFMPEG_VERSION}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/ffmpeg-source-offer.md.in"
    "${PROJECT_BINARY_DIR}/FFMPEG_SOURCE_OFFER.md"
    @ONLY
)

set(edit_atlas_vcpkg_share_directory
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share"
)
foreach(
    edit_atlas_qt_package
    IN ITEMS
        qtbase
        qtdeclarative
        qtlanguageserver
        qtshadertools
        qtsvg
)
    set(
        edit_atlas_qt_copyright
        "${edit_atlas_vcpkg_share_directory}/${edit_atlas_qt_package}/copyright"
    )
    if(NOT EXISTS "${edit_atlas_qt_copyright}")
        message(
            FATAL_ERROR
            "The resolved ${edit_atlas_qt_package} copyright file was not "
            "found at ${edit_atlas_qt_copyright}."
        )
    endif()
endforeach()
set(edit_atlas_ffmpeg_copyright
    "${edit_atlas_vcpkg_share_directory}/ffmpeg/copyright"
)
if(NOT EXISTS "${edit_atlas_ffmpeg_copyright}")
    message(
        FATAL_ERROR
        "The resolved FFmpeg copyright file was not found at "
        "${edit_atlas_ffmpeg_copyright}."
    )
endif()

add_subdirectory(src/core)
add_subdirectory(src/media)
add_subdirectory(src/storage)
add_subdirectory(src/formats/cmx3600)
add_subdirectory(src/formats/xlsx)
add_subdirectory(src/services)
add_subdirectory(src/support)
add_subdirectory(src/presentation)
add_subdirectory(src/frontends)

qt_add_translations(
    TARGETS edit_atlas_presentation
    SOURCE_TARGETS
        edit_atlas_presentation
        edit_atlas_quick_frontend
        edit_atlas_widgets_frontend
    TS_FILES src/presentation/translations/edit_atlas_pt_BR.ts
    RESOURCE_PREFIX "/i18n"
)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    set(edit_atlas_test_qt_components Test)
    if(
        EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "all"
        OR EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "quick"
    )
        list(APPEND edit_atlas_test_qt_components QuickTest)
    endif()
    find_package(Qt6 REQUIRED COMPONENTS ${edit_atlas_test_qt_components})
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
        "${PROJECT_BINARY_DIR}/FFMPEG_SOURCE_OFFER.md"
        "${PROJECT_BINARY_DIR}/QT_SOURCE_OFFER.md"
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
