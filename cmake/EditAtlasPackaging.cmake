set(CPACK_PACKAGE_NAME "Edit Atlas")
set(CPACK_PACKAGE_VENDOR "Edit Atlas contributors")
set(CPACK_PACKAGE_CONTACT
    "https://github.com/gabrielslrodrigues/edit-atlas/issues"
)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL
    "https://github.com/gabrielslrodrigues/edit-atlas"
)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Edit Atlas")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_THREADS 0)
set(CPACK_VERBATIM_VARIABLES ON)

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/EditAtlasCPackOptions.cmake.in"
    "${PROJECT_BINARY_DIR}/EditAtlasCPackOptions.cmake"
    @ONLY
)
set(CPACK_PROJECT_CONFIG_FILE
    "${PROJECT_BINARY_DIR}/EditAtlasCPackOptions.cmake"
)

if(WIN32)
    configure_file(
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_BINARY_DIR}/EditAtlasLicense.txt"
        COPYONLY
    )
    set(CPACK_RESOURCE_FILE_LICENSE
        "${PROJECT_BINARY_DIR}/EditAtlasLicense.txt"
    )
    set(CPACK_GENERATOR "WIX")
    set(CPACK_PACKAGE_FILE_NAME
        "edit-atlas-${PROJECT_VERSION}-windows-x64"
    )
    set(CPACK_PACKAGE_EXECUTABLES "edit-atlas" "Edit Atlas")
    set(CPACK_WIX_VERSION 3)
    set(CPACK_WIX_ARCHITECTURE x64)
    set(CPACK_WIX_INSTALL_SCOPE perMachine)
    set(CPACK_WIX_COMPONENT_INSTALL OFF)
    set(CPACK_WIX_UPGRADE_GUID
        "01BB5D14-C875-4EC5-9F30-1E31C2791DC7"
    )
    set(CPACK_WIX_PRODUCT_ICON
        "${PROJECT_SOURCE_DIR}/src/app/resources/icons/edit_atlas.ico"
    )
    set(CPACK_WIX_UI_REF "WixUI_InstallDir")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Edit Atlas")
    set(CPACK_WIX_PROPERTY_ARPCOMMENTS
        "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}"
    )
    set(CPACK_WIX_PROPERTY_ARPHELPLINK "${CPACK_PACKAGE_CONTACT}")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT
        "${CPACK_PACKAGE_HOMEPAGE_URL}"
    )
elseif(APPLE)
    set(CPACK_GENERATOR "productbuild")
    set(CPACK_PACKAGE_FILE_NAME
        "edit-atlas-${PROJECT_VERSION}-macos-universal"
    )
    set(CPACK_PRODUCTBUILD_IDENTIFIER
        "com.github.gabrielslrodrigues.edit-atlas"
    )
    set(CPACK_PRODUCTBUILD_COMPONENT_INSTALL OFF)
    set(CPACK_PRODUCTBUILD_DOMAINS ON)
    set(CPACK_PRODUCTBUILD_DOMAINS_ANYWHERE OFF)
    set(CPACK_PRODUCTBUILD_DOMAINS_USER OFF)
    set(CPACK_PRODUCTBUILD_DOMAINS_ROOT ON)
elseif(LINUX)
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_PACKAGE_FILE_NAME
        "edit-atlas-${PROJECT_VERSION}-linux-x86_64"
    )
    set(CPACK_DEBIAN_FILE_NAME
        "edit-atlas-${PROJECT_VERSION}-linux-x86_64.deb"
    )
    set(CPACK_DEBIAN_PACKAGE_NAME "edit-atlas")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_VENDOR}")
    set(CPACK_DEBIAN_PACKAGE_SECTION "video")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.39)")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_RPM_FILE_NAME
        "edit-atlas-${PROJECT_VERSION}-linux-x86_64.rpm"
    )
    set(CPACK_RPM_PACKAGE_NAME "edit-atlas")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
    set(CPACK_RPM_PACKAGE_LICENSE
        "Apache-2.0 AND LGPL-3.0-only AND LGPL-2.1-or-later AND LicenseRef-ThirdParty"
    )
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
    set(CPACK_RPM_PACKAGE_REQUIRES "glibc >= 2.39")
    set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
else()
    message(FATAL_ERROR "Edit Atlas packaging is unsupported on this platform.")
endif()

include(CPack)
