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
