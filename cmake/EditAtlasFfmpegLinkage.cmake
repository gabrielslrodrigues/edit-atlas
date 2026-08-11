include_guard(GLOBAL)

function(edit_atlas_require_dynamic_ffmpeg)
    if(WIN32)
        set(
            edit_atlas_ffmpeg_runtime_directory
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
        )
        foreach(
            edit_atlas_ffmpeg_component
            IN ITEMS avcodec avformat avutil swscale
        )
            file(
                GLOB edit_atlas_ffmpeg_runtime_libraries
                LIST_DIRECTORIES FALSE
                "${edit_atlas_ffmpeg_runtime_directory}/${edit_atlas_ffmpeg_component}-*.dll"
            )
            if(NOT edit_atlas_ffmpeg_runtime_libraries)
                message(
                    FATAL_ERROR
                    "FFmpeg component ${edit_atlas_ffmpeg_component} must be "
                    "a shared library. Configure with the project-owned "
                    "dynamic vcpkg triplets."
                )
            endif()
        endforeach()
        return()
    endif()

    foreach(
        edit_atlas_ffmpeg_library_variable
        IN ITEMS
            FFMPEG_libavcodec_LIBRARY_RELEASE
            FFMPEG_libavformat_LIBRARY_RELEASE
            FFMPEG_libavutil_LIBRARY_RELEASE
            FFMPEG_libswscale_LIBRARY_RELEASE
    )
        set(
            edit_atlas_ffmpeg_library
            "${${edit_atlas_ffmpeg_library_variable}}"
        )
        if(
            NOT edit_atlas_ffmpeg_library
            OR edit_atlas_ffmpeg_library MATCHES "\\.a$"
        )
            message(
                FATAL_ERROR
                "${edit_atlas_ffmpeg_library_variable} must resolve to a "
                "shared library, but resolved to "
                "${edit_atlas_ffmpeg_library}. Configure with the "
                "project-owned dynamic vcpkg triplets."
            )
        endif()
    endforeach()
endfunction()

function(edit_atlas_install_ffmpeg_runtime destination)
    if(WIN32)
        set(edit_atlas_ffmpeg_runtime_subdirectory "bin")
        set(edit_atlas_ffmpeg_runtime_prefix "")
        set(edit_atlas_ffmpeg_runtime_suffix "-*.dll")
    elseif(APPLE)
        set(edit_atlas_ffmpeg_runtime_subdirectory "lib")
        set(edit_atlas_ffmpeg_runtime_prefix "lib")
        set(edit_atlas_ffmpeg_runtime_suffix "*.dylib")
    elseif(LINUX)
        set(edit_atlas_ffmpeg_runtime_subdirectory "lib")
        set(edit_atlas_ffmpeg_runtime_prefix "lib")
        set(edit_atlas_ffmpeg_runtime_suffix ".so*")
    else()
        return()
    endif()

    foreach(edit_atlas_ffmpeg_runtime_kind IN ITEMS release debug)
        if(edit_atlas_ffmpeg_runtime_kind STREQUAL "debug")
            set(edit_atlas_ffmpeg_runtime_configuration_directory "debug/")
            set(edit_atlas_ffmpeg_runtime_configurations Debug)
        else()
            set(edit_atlas_ffmpeg_runtime_configuration_directory "")
            set(
                edit_atlas_ffmpeg_runtime_configurations
                MinSizeRel
                Release
                RelWithDebInfo
            )
        endif()

        string(
            CONCAT
            edit_atlas_ffmpeg_runtime_directory
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/"
            "${edit_atlas_ffmpeg_runtime_configuration_directory}"
            "${edit_atlas_ffmpeg_runtime_subdirectory}"
        )
        set(edit_atlas_ffmpeg_runtime_files)
        foreach(
            edit_atlas_ffmpeg_component
            IN ITEMS avcodec avformat avutil swscale
        )
            string(
                CONCAT edit_atlas_ffmpeg_runtime_pattern
                "${edit_atlas_ffmpeg_runtime_directory}/"
                "${edit_atlas_ffmpeg_runtime_prefix}"
                "${edit_atlas_ffmpeg_component}"
                "${edit_atlas_ffmpeg_runtime_suffix}"
            )
            file(
                GLOB edit_atlas_ffmpeg_component_runtime_files
                LIST_DIRECTORIES FALSE
                "${edit_atlas_ffmpeg_runtime_pattern}"
            )
            if(NOT edit_atlas_ffmpeg_component_runtime_files)
                message(
                    FATAL_ERROR
                    "No ${edit_atlas_ffmpeg_runtime_kind} runtime files were "
                    "found for FFmpeg component "
                    "${edit_atlas_ffmpeg_component} under "
                    "${edit_atlas_ffmpeg_runtime_directory}."
                )
            endif()
            list(
                APPEND edit_atlas_ffmpeg_runtime_files
                ${edit_atlas_ffmpeg_component_runtime_files}
            )
        endforeach()

        install(
            FILES ${edit_atlas_ffmpeg_runtime_files}
            DESTINATION "${destination}"
            CONFIGURATIONS ${edit_atlas_ffmpeg_runtime_configurations}
            COMPONENT Runtime
        )
    endforeach()
endfunction()
