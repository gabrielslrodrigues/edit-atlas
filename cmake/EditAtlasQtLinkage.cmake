include_guard(GLOBAL)

function(edit_atlas_require_dynamic_qt)
    foreach(
        edit_atlas_qt_target
        IN ITEMS
            Qt6::Concurrent
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
            Qt6::QuickControls2
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

function(edit_atlas_normalize_macos_qt_dependencies target)
    if(NOT APPLE)
        return()
    endif()

    add_custom_command(
        TARGET "${target}"
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}"
            "-DEDIT_ATLAS_EXECUTABLE=$<TARGET_FILE:${target}>"
            -P
            "${PROJECT_SOURCE_DIR}/cmake/NormalizeMacOSQtDependencies.cmake"
        VERBATIM
    )
endfunction()

function(
    edit_atlas_deploy_windows_qt_platform_plugin
    target
    platform_plugin
)
    if(NOT WIN32)
        return()
    endif()

    if(NOT platform_plugin)
        set(platform_plugin Qt6::QWindowsIntegrationPlugin)
    endif()
    string(
        CONCAT
        edit_atlas_qt_platform_plugin_destination
        "$<TARGET_FILE_DIR:${target}>/platforms/"
        "$<TARGET_FILE_NAME:${platform_plugin}>"
    )

    add_custom_command(
        TARGET "${target}"
        POST_BUILD
        COMMAND
            "${CMAKE_COMMAND}" -E make_directory
            "$<TARGET_FILE_DIR:${target}>/platforms"
        COMMAND
            "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${platform_plugin}>"
            "${edit_atlas_qt_platform_plugin_destination}"
        VERBATIM
    )
endfunction()
