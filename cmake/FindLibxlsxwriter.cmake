if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(
        _Libxlsxwriter_ROOT
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}"
    )

    find_path(
        Libxlsxwriter_INCLUDE_DIR
        NAMES xlsxwriter.h
        PATHS "${_Libxlsxwriter_ROOT}/include"
        NO_DEFAULT_PATH
        DOC "libxlsxwriter include directory"
    )

    find_library(
        Libxlsxwriter_LIBRARY_RELEASE
        NAMES xlsxwriter
        PATHS "${_Libxlsxwriter_ROOT}/lib"
        NO_DEFAULT_PATH
        DOC "libxlsxwriter release library"
    )

    find_library(
        Libxlsxwriter_LIBRARY_DEBUG
        NAMES xlsxwriter
        PATHS "${_Libxlsxwriter_ROOT}/debug/lib"
        NO_DEFAULT_PATH
        DOC "libxlsxwriter debug library"
    )
else()
    find_path(
        Libxlsxwriter_INCLUDE_DIR
        NAMES xlsxwriter.h
        DOC "libxlsxwriter include directory"
    )

    find_library(
        Libxlsxwriter_LIBRARY_RELEASE
        NAMES xlsxwriter
        DOC "libxlsxwriter release library"
    )
    set(
        Libxlsxwriter_LIBRARY_DEBUG
        "${Libxlsxwriter_LIBRARY_RELEASE}"
        CACHE FILEPATH
        "libxlsxwriter debug library"
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Libxlsxwriter
    REQUIRED_VARS
        Libxlsxwriter_INCLUDE_DIR
        Libxlsxwriter_LIBRARY_RELEASE
        Libxlsxwriter_LIBRARY_DEBUG
)

if(Libxlsxwriter_FOUND AND NOT TARGET Libxlsxwriter::Libxlsxwriter)
    add_library(Libxlsxwriter::Libxlsxwriter UNKNOWN IMPORTED)
    set_target_properties(
        Libxlsxwriter::Libxlsxwriter
        PROPERTIES
            IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
            IMPORTED_LOCATION_DEBUG "${Libxlsxwriter_LIBRARY_DEBUG}"
            IMPORTED_LOCATION_RELEASE "${Libxlsxwriter_LIBRARY_RELEASE}"
            MAP_IMPORTED_CONFIG_MINSIZEREL Release
            MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
            INTERFACE_INCLUDE_DIRECTORIES "${Libxlsxwriter_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(
    Libxlsxwriter_INCLUDE_DIR
    Libxlsxwriter_LIBRARY_DEBUG
    Libxlsxwriter_LIBRARY_RELEASE
)

unset(_Libxlsxwriter_ROOT)
