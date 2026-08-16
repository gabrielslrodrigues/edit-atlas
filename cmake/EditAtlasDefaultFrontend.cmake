include_guard(GLOBAL)

function(edit_atlas_define_default_frontend frontend)
    if(frontend STREQUAL "quick")
        set(target edit_atlas_quick)
        set(edit_atlas_requires_qml_deployment TRUE)
    elseif(frontend STREQUAL "widgets")
        set(target edit_atlas_widgets)
        set(edit_atlas_requires_qml_deployment FALSE)
    else()
        message(
            FATAL_ERROR
            "Unsupported Edit Atlas frontend: ${frontend}. "
            "Expected quick or widgets."
        )
    endif()

    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Unknown target for default frontend: ${target}")
    endif()

    add_executable(EditAtlas::Application ALIAS "${target}")

    set(edit_atlas_resource_directory
        "${PROJECT_SOURCE_DIR}/src/frontends/resources"
    )
    qt_add_resources(
        "${target}"
        "edit_atlas_icons"
        PREFIX
            "/"
        BASE
            "${edit_atlas_resource_directory}"
        FILES
            "${edit_atlas_resource_directory}/icons/edit_atlas.png"
    )

    set_target_properties("${target}" PROPERTIES OUTPUT_NAME "edit-atlas")

    if(WIN32)
        set(edit_atlas_windows_icon
            "${edit_atlas_resource_directory}/icons/edit_atlas.ico"
        )
        configure_file(
            "${edit_atlas_resource_directory}/icons/edit_atlas.rc.in"
            "${PROJECT_BINARY_DIR}/edit_atlas.rc"
            @ONLY
        )
        target_sources(
            "${target}"
            PRIVATE
                "${PROJECT_BINARY_DIR}/edit_atlas.rc"
        )
        set_target_properties(
            "${target}"
            PROPERTIES
                WIN32_EXECUTABLE ON
        )
    elseif(APPLE)
        set(edit_atlas_macos_icon
            "${edit_atlas_resource_directory}/icons/edit_atlas.icns"
        )
        set_source_files_properties(
            "${edit_atlas_macos_icon}"
            PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources("${target}" PRIVATE "${edit_atlas_macos_icon}")
        set_target_properties(
            "${target}"
            PROPERTIES
                MACOSX_BUNDLE ON
                MACOSX_BUNDLE_BUNDLE_NAME "Edit Atlas"
                MACOSX_BUNDLE_GUI_IDENTIFIER
                    "com.github.gabrielslrodrigues.edit-atlas"
                MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
                MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
                MACOSX_BUNDLE_ICON_FILE "edit_atlas.icns"
                INSTALL_RPATH "@loader_path/../Frameworks"
        )
    elseif(LINUX)
        set_target_properties(
            "${target}"
            PROPERTIES
                INSTALL_RPATH
                    "$ORIGIN/../${edit_atlas_runtime_install_libdir}"
        )
    endif()

    set(edit_atlas_runtime_dependency_arguments)
    if(WIN32)
        list(
            APPEND edit_atlas_runtime_dependency_arguments
            RUNTIME_DEPENDENCY_SET edit_atlas_runtime_dependencies
        )
    endif()

    install(
        TARGETS "${target}"
        ${edit_atlas_runtime_dependency_arguments}
        BUNDLE
            DESTINATION "."
            COMPONENT Runtime
        RUNTIME
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT Runtime
    )

    if(WIN32)
        install(
            RUNTIME_DEPENDENCY_SET edit_atlas_runtime_dependencies
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT Runtime
            DIRECTORIES
                "$<IF:$<CONFIG:Debug>,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin>"
            PRE_EXCLUDE_REGEXES
                "api-ms-.*"
                "ext-ms-.*"
            POST_EXCLUDE_REGEXES
                ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
        )
    elseif(APPLE)
        install(
            FILES
                "$<TARGET_FILE:spdlog::spdlog>"
                "$<TARGET_FILE:fmt::fmt>"
            DESTINATION "edit-atlas.app/Contents/Frameworks"
            COMPONENT Runtime
        )
    elseif(LINUX)
        install(
            FILES
                "${edit_atlas_resource_directory}/linux/edit-atlas.desktop"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
            COMPONENT Runtime
        )
        install(
            FILES "${edit_atlas_resource_directory}/icons/edit_atlas.png"
            DESTINATION
                "${CMAKE_INSTALL_DATADIR}/icons/hicolor/1024x1024/apps"
            RENAME "edit-atlas.png"
            COMPONENT Runtime
        )
    endif()

    if(LINUX)
        set(
            edit_atlas_runtime_install_pluginsdir
            "${edit_atlas_runtime_install_libdir}/Qt6/plugins"
        )
        set(edit_atlas_linux_qml_deployment)
        set(edit_atlas_linux_additional_modules)
        if(edit_atlas_requires_qml_deployment)
            set(
                edit_atlas_runtime_install_qmldir
                "${edit_atlas_runtime_install_libdir}/Qt6/qml"
            )
            set(
                edit_atlas_linux_qml_deployment
                "set(
                    QT_DEPLOY_QML_DIR
                    \"${edit_atlas_runtime_install_qmldir}\"
                )
                qt_deploy_qml_imports(
                    TARGET ${target}
                    PLUGINS_FOUND edit_atlas_qml_plugins
                )"
            )
            set(
                edit_atlas_linux_additional_modules
                "ADDITIONAL_MODULES \${edit_atlas_qml_plugins}"
            )
        endif()
        qt_generate_deploy_script(
            TARGET "${target}"
            OUTPUT_SCRIPT edit_atlas_deploy_script
            CONTENT
                "set(
                    QT_DEPLOY_LIB_DIR
                    \"${edit_atlas_runtime_install_libdir}\"
                )
                set(
                    QT_DEPLOY_PLUGINS_DIR
                    \"${edit_atlas_runtime_install_pluginsdir}\"
                )
                ${edit_atlas_linux_qml_deployment}
                qt_deploy_runtime_dependencies(
                    EXECUTABLE \"$<TARGET_FILE:${target}>\"
                    ${edit_atlas_linux_additional_modules}
                    GENERATE_QT_CONF
                    LIB_DIR \"${edit_atlas_runtime_install_libdir}\"
                    PLUGINS_DIR \"${edit_atlas_runtime_install_pluginsdir}\"
                    INCLUDE_PLUGINS qwayland qxcb
                )"
        )
    elseif(APPLE)
        set(
            edit_atlas_deploy_script_arguments
            DEPLOY_TOOL_OPTIONS
                "-libpath=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
        )
    elseif(WIN32)
        set(edit_atlas_deploy_script_arguments)
    else()
        message(
            FATAL_ERROR
            "Default frontend deployment is unsupported on this platform."
        )
    endif()

    if(NOT LINUX)
        if(edit_atlas_requires_qml_deployment)
            set(
                edit_atlas_deploy_script_command
                qt_generate_deploy_qml_app_script
            )
        else()
            set(
                edit_atlas_deploy_script_command
                qt_generate_deploy_app_script
            )
        endif()
        cmake_language(
            CALL ${edit_atlas_deploy_script_command}
            TARGET "${target}"
            OUTPUT_SCRIPT edit_atlas_deploy_script
            NO_UNSUPPORTED_PLATFORM_ERROR
            ${edit_atlas_deploy_script_arguments}
        )
    endif()
    install(
        SCRIPT "${edit_atlas_deploy_script}"
        COMPONENT Runtime
    )
endfunction()

function(edit_atlas_install_cli_application target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Unknown CLI application target: ${target}")
    endif()
    set(edit_atlas_cli_install_directory "${CMAKE_INSTALL_BINDIR}")
    if(APPLE)
        set_target_properties(
            "${target}"
            PROPERTIES
                INSTALL_RPATH "@loader_path/../Frameworks"
        )
        set(edit_atlas_cli_install_directory
            "edit-atlas.app/Contents/MacOS"
        )
    elseif(LINUX)
        set_target_properties(
            "${target}"
            PROPERTIES
                INSTALL_RPATH
                    "$ORIGIN/../${edit_atlas_runtime_install_libdir}"
        )
    endif()

    install(
        TARGETS "${target}"
        RUNTIME
            DESTINATION "${edit_atlas_cli_install_directory}"
            COMPONENT Runtime
    )
endfunction()
