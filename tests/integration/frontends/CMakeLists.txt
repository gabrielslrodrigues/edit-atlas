add_subdirectory(cli)

if(
    EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "all"
    OR EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "quick"
)
    add_subdirectory(quick)
endif()

if(
    EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "all"
    OR EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "widgets"
)
    add_subdirectory(widgets)
endif()
