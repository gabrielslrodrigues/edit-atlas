include_guard(GLOBAL)

function(edit_atlas_deploy_linux_qml_plugin_dependencies)
    set(
        single_value_options
        QML_DIRECTORY
        QML_SOURCE_DIRECTORY
        LIBRARY_DIRECTORY
        LIBRARY_SOURCE_DIRECTORY
    )
    cmake_parse_arguments(
        PARSE_ARGV 0
        edit_atlas_qml
        ""
        "${single_value_options}"
        ""
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

    set(
        edit_atlas_deployed_qml_root
        "${QT_DEPLOY_PREFIX}/${edit_atlas_qml_QML_DIRECTORY}"
    )
    file(
        GLOB_RECURSE edit_atlas_deployed_qml_plugins
        LIST_DIRECTORIES FALSE
        "${edit_atlas_deployed_qml_root}/*.so"
    )
    if(NOT edit_atlas_deployed_qml_plugins)
        message(
            FATAL_ERROR
            "No QML plugins were deployed under "
            "${edit_atlas_deployed_qml_root}."
        )
    endif()

    set(edit_atlas_source_qml_plugins)
    foreach(plugin IN LISTS edit_atlas_deployed_qml_plugins)
        file(
            RELATIVE_PATH edit_atlas_qml_plugin_relative_path
            "${edit_atlas_deployed_qml_root}"
            "${plugin}"
        )
        string(
            CONCAT edit_atlas_source_qml_plugin
            "${edit_atlas_qml_QML_SOURCE_DIRECTORY}/"
            "${edit_atlas_qml_plugin_relative_path}"
        )
        if(EXISTS "${edit_atlas_source_qml_plugin}")
            list(
                APPEND edit_atlas_source_qml_plugins
                "${edit_atlas_source_qml_plugin}"
            )
        endif()
    endforeach()
    if(NOT edit_atlas_source_qml_plugins)
        message(
            FATAL_ERROR
            "No deployed QML plugins matched the Qt installation under "
            "${edit_atlas_qml_QML_SOURCE_DIRECTORY}."
        )
    endif()

    file(
        GET_RUNTIME_DEPENDENCIES
        MODULES ${edit_atlas_source_qml_plugins}
        RESOLVED_DEPENDENCIES_VAR edit_atlas_qml_dependencies
        UNRESOLVED_DEPENDENCIES_VAR edit_atlas_qml_unresolved_dependencies
        CONFLICTING_DEPENDENCIES_PREFIX edit_atlas_qml_conflicting
    )
    if(edit_atlas_qml_unresolved_dependencies)
        message(
            FATAL_ERROR
            "Unresolved QML plugin dependencies: "
            "${edit_atlas_qml_unresolved_dependencies}"
        )
    endif()
    if(edit_atlas_qml_conflicting_FILENAMES)
        message(
            FATAL_ERROR
            "Conflicting QML plugin dependencies: "
            "${edit_atlas_qml_conflicting_FILENAMES}"
        )
    endif()

    foreach(dependency IN LISTS edit_atlas_qml_dependencies)
        cmake_path(
            IS_PREFIX edit_atlas_qml_LIBRARY_SOURCE_DIRECTORY
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
