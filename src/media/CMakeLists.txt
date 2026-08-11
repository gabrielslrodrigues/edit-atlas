add_library(edit_atlas_media STATIC)
add_library(EditAtlas::Media ALIAS edit_atlas_media)

set_target_properties(
    edit_atlas_media
    PROPERTIES
        EXPORT_NAME Media
        POSITION_INDEPENDENT_CODE ON
)

target_sources(
    edit_atlas_media
    PRIVATE
        ffmpeg_video_decoder.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/edit_atlas/media/video_decoder.hpp
)

target_include_directories(
    edit_atlas_media
    SYSTEM PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(
    edit_atlas_media
    PUBLIC EditAtlas::Core
    PRIVATE ${FFMPEG_LIBRARIES}
)
target_compile_features(edit_atlas_media PUBLIC cxx_std_23)
edit_atlas_set_project_warnings(edit_atlas_media)

install(
    TARGETS edit_atlas_media
    EXPORT EditAtlasTargets
    FILE_SET HEADERS
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT Development
    ARCHIVE
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT Development
    LIBRARY
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT Development
)

if(WIN32)
    set(edit_atlas_ffmpeg_runtime_destination "${CMAKE_INSTALL_BINDIR}")
elseif(APPLE)
    set(edit_atlas_ffmpeg_runtime_destination
        "edit-atlas.app/Contents/Frameworks"
    )
elseif(LINUX)
    set(edit_atlas_ffmpeg_runtime_destination
        "${edit_atlas_runtime_install_libdir}"
    )
endif()

if(DEFINED edit_atlas_ffmpeg_runtime_destination)
    edit_atlas_install_ffmpeg_runtime(
        "${edit_atlas_ffmpeg_runtime_destination}"
    )
endif()
