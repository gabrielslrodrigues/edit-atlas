include_guard(GLOBAL)

function(edit_atlas_deploy_linux_qml_plugin_dependencies)
    set(single_value_options LIBRARY_DIRECTORY SOURCE_DIRECTORY)
    cmake_parse_arguments(
        PARSE_ARGV 0
        edit_atlas_qml
        ""
        "${single_value_options}"
        "PLUGINS"
    )

    foreach(option IN LISTS single_value_options)
        if(NOT edit_atlas_qml_${option})
            message(FATAL_ERROR "${option} is required.")
        endif()
    endforeach()
    if(edit_atlas_qml_UNPARSED_ARGUMENTS)
        message(
            FATAL_ERROR
            "Unexpected arguments: ${edit_atlas_qml_UNPARSED_ARGUMENTS}"
        )
    endif()
    if(NOT edit_atlas_qml_PLUGINS)
        message(
            FATAL_ERROR
            "At least one QML plugin is required."
        )
    endif()

    file(
        GET_RUNTIME_DEPENDENCIES
        MODULES ${edit_atlas_qml_PLUGINS}
        RESOLVED_DEPENDENCIES_VAR edit_atlas_qml_dependencies
        UNRESOLVED_DEPENDENCIES_VAR edit_atlas_qml_unresolved_dependencies
        CONFLICTING_DEPENDENCIES_PREFIX edit_atlas_qml_conflicting
    )
    if(edit_atlas_qml_unresolved_dependencies)
        message(
            FATAL_ERROR
            "Unresolved deployed QML plugin dependencies: "
            "${edit_atlas_qml_unresolved_dependencies}"
        )
    endif()
    if(edit_atlas_qml_conflicting_FILENAMES)
        message(
            FATAL_ERROR
            "Conflicting deployed QML plugin dependencies: "
            "${edit_atlas_qml_conflicting_FILENAMES}"
        )
    endif()

    foreach(dependency IN LISTS edit_atlas_qml_dependencies)
        cmake_path(
            IS_PREFIX edit_atlas_qml_SOURCE_DIRECTORY
            "${dependency}"
            NORMALIZE
            edit_atlas_qml_is_vcpkg_dependency
        )
        if(edit_atlas_qml_is_vcpkg_dependency)
            file(
                INSTALL "${dependency}"
                DESTINATION
                    "${CMAKE_INSTALL_PREFIX}/${edit_atlas_qml_LIBRARY_DIRECTORY}"
                FOLLOW_SYMLINK_CHAIN
            )
        endif()
    endforeach()
endfunction()
