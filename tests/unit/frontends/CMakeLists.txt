if(
    EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "all"
    OR EDIT_ATLAS_BUILD_FRONTENDS STREQUAL "widgets"
)
    add_subdirectory(widgets)
endif()
