cmake_minimum_required(VERSION 4.0)

project(
    EditAtlas
    VERSION 0.1.0
    DESCRIPTION "Inspect editorial timelines and export structured reports"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

option(
    EDIT_ATLAS_WARNINGS_AS_ERRORS
    "Treat compiler warnings from Edit Atlas targets as errors"
    OFF
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EditAtlasWarnings)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(spdlog CONFIG REQUIRED)

add_subdirectory(src/core)
