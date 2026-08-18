include_guard(GLOBAL)

function(edit_atlas_configure_product_application target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Unknown application target: ${target}")
    endif()

    set(
        edit_atlas_resource_directory
        "${PROJECT_SOURCE_DIR}/src/frontends/resources"
    )
    qt_add_resources(
        "${target}"
        "${target}_icons"
        PREFIX
            "/"
        BASE
            "${edit_atlas_resource_directory}"
        FILES
            "${edit_atlas_resource_directory}/icons/edit_atlas.png"
    )

    set_target_properties("${target}" PROPERTIES OUTPUT_NAME "edit-atlas")

    if(WIN32)
        set(
            edit_atlas_windows_icon
            "${edit_atlas_resource_directory}/icons/edit_atlas.ico"
        )
        set(edit_atlas_resource_file "${PROJECT_BINARY_DIR}/${target}.rc")
        configure_file(
            "${edit_atlas_resource_directory}/icons/edit_atlas.rc.in"
            "${edit_atlas_resource_file}"
            @ONLY
        )
        target_sources("${target}" PRIVATE "${edit_atlas_resource_file}")
        set_target_properties("${target}" PROPERTIES WIN32_EXECUTABLE ON)
    elseif(APPLE)
        set(
            edit_atlas_macos_icon
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
endfunction()

function(
    edit_atlas_install_frontend_application
    frontend
    target
    component
    exclude_from_all
)
    if(frontend STREQUAL "quick")
        set(edit_atlas_requires_qml_deployment TRUE)
    elseif(frontend STREQUAL "widgets")
        set(edit_atlas_requires_qml_deployment FALSE)
    else()
        message(
            FATAL_ERROR
            "Unsupported Edit Atlas frontend: ${frontend}. "
            "Expected quick or widgets."
        )
    endif()

    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Unknown application target: ${target}")
    endif()

    set(edit_atlas_install_exclusion)
    if(exclude_from_all)
        set(edit_atlas_install_exclusion EXCLUDE_FROM_ALL)
    endif()

    set(edit_atlas_runtime_dependency_arguments)
    if(WIN32)
        set(
            edit_atlas_runtime_dependency_set
            "${target}_runtime_dependencies"
        )
        list(
            APPEND edit_atlas_runtime_dependency_arguments
            RUNTIME_DEPENDENCY_SET
                "${edit_atlas_runtime_dependency_set}"
        )
    endif()

    install(
        TARGETS "${target}"
        ${edit_atlas_runtime_dependency_arguments}
        BUNDLE
            DESTINATION "."
            COMPONENT "${component}"
            ${edit_atlas_install_exclusion}
        RUNTIME
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT "${component}"
            ${edit_atlas_install_exclusion}
    )

    if(WIN32)
        install(
            RUNTIME_DEPENDENCY_SET "${edit_atlas_runtime_dependency_set}"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT "${component}"
            ${edit_atlas_install_exclusion}
            DIRECTORIES
                "$<IF:$<CONFIG:Debug>,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin,${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin>"
            PRE_EXCLUDE_REGEXES
                "api-ms-.*"
                "ext-ms-.*"
            POST_EXCLUDE_REGEXES
                ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
        )
    endif()

    if(LINUX)
        set(
            edit_atlas_runtime_install_pluginsdir
            "${edit_atlas_runtime_install_libdir}/Qt6/plugins"
        )
        set(edit_atlas_linux_qml_deployment)
        set(edit_atlas_linux_additional_modules)
        set(edit_atlas_linux_qml_dependency_deployment)
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
                # Qt 6.11's versionless wrapper loses PLUGINS_FOUND across
                # its additional function scope in deployment-script mode.
                qt6_deploy_qml_imports(
                    TARGET ${target}
                    PLUGINS_FOUND edit_atlas_qml_plugins
                )"
            )
            set(
                edit_atlas_linux_additional_modules
                "ADDITIONAL_MODULES \${edit_atlas_qml_plugins}"
            )
            set(
                edit_atlas_linux_qml_dependency_deployment
                "include(
                    \"${PROJECT_SOURCE_DIR}/cmake/DeployLinuxQmlPluginDependencies.cmake\"
                )
                edit_atlas_deploy_linux_qml_plugin_dependencies(
                    QML_DIRECTORY
                        \"${edit_atlas_runtime_install_qmldir}\"
                    QML_SOURCE_DIRECTORY
                        \"${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6/qml\"
                    LIBRARY_DIRECTORY
                        \"${edit_atlas_runtime_install_libdir}\"
                    LIBRARY_SOURCE_DIRECTORY
                        \"${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib\"
                )"
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
                )
                ${edit_atlas_linux_qml_dependency_deployment}"
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
            "Frontend deployment is unsupported on this platform."
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
        COMPONENT "${component}"
        ${edit_atlas_install_exclusion}
    )
endfunction()

function(edit_atlas_define_packaged_frontend frontend target)
    if(frontend STREQUAL "quick")
        set(edit_atlas_component QuickRuntime)
    elseif(frontend STREQUAL "widgets")
        set(edit_atlas_component WidgetsRuntime)
    else()
        message(
            FATAL_ERROR
            "Unsupported Edit Atlas frontend: ${frontend}. "
            "Expected quick or widgets."
        )
    endif()

    edit_atlas_configure_product_application("${target}")
    # Package applications must not collide with the normal default frontend
    # during an unqualified `cmake --install` invocation.
    edit_atlas_install_frontend_application(
        "${frontend}"
        "${target}"
        "${edit_atlas_component}"
        TRUE
    )
endfunction()

function(edit_atlas_install_shared_application_runtime)
    set(
        edit_atlas_resource_directory
        "${PROJECT_SOURCE_DIR}/src/frontends/resources"
    )
    if(APPLE)
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
endfunction()

function(edit_atlas_define_default_frontend frontend)
    if(frontend STREQUAL "quick")
        set(target edit_atlas_quick)
    elseif(frontend STREQUAL "widgets")
        set(target edit_atlas_widgets)
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
    edit_atlas_configure_product_application("${target}")
    edit_atlas_install_frontend_application(
        "${frontend}"
        "${target}"
        DefaultFrontendRuntime
        FALSE
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
