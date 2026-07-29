include_guard(GLOBAL)

function(edit_atlas_require_dynamic_qt)
    foreach(
        edit_atlas_qt_target
        IN ITEMS
            Qt6::Concurrent
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
    )
        get_target_property(
            edit_atlas_qt_target_type
            "${edit_atlas_qt_target}"
            TYPE
        )
        if(NOT edit_atlas_qt_target_type STREQUAL "SHARED_LIBRARY")
            message(
                FATAL_ERROR
                "${edit_atlas_qt_target} must be a shared library, but its "
                "imported target type is ${edit_atlas_qt_target_type}. "
                "Configure with the project-owned dynamic vcpkg triplets."
            )
        endif()
    endforeach()
endfunction()
