# Builds include-what-you-use from a pinned commit against an installed LLVM
# and installs it, with its Qt mapping generator, into a prefix.
#
#   cmake -DEDIT_ATLAS_IWYU_COMMIT=<sha> -DEDIT_ATLAS_IWYU_LLVM_PREFIX=<dir>
#         -DEDIT_ATLAS_IWYU_PREFIX=<dir> -P cmake/BuildIncludeWhatYouUse.cmake
#
# The project configure runs this when EDIT_ATLAS_INCLUDE_WHAT_YOU_USE is on
# and the prefix holds no build yet; see EditAtlasIncludeWhatYouUse.cmake for
# the commit chosen for each Clang release.

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/MinimumVersion.txt"
    EDIT_ATLAS_MINIMUM_CMAKE_VERSION)
cmake_minimum_required(VERSION ${EDIT_ATLAS_MINIMUM_CMAKE_VERSION})

foreach(
    variable
    IN ITEMS
        EDIT_ATLAS_IWYU_COMMIT
        EDIT_ATLAS_IWYU_LLVM_PREFIX
        EDIT_ATLAS_IWYU_PREFIX
)
    if(NOT DEFINED "${variable}")
        message(FATAL_ERROR "${variable} is required.")
    endif()
endforeach()

find_program(edit_atlas_iwyu_git git REQUIRED)
find_program(edit_atlas_iwyu_ninja ninja REQUIRED)

set(work "${EDIT_ATLAS_IWYU_PREFIX}.work")
file(REMOVE_RECURSE "${work}" "${EDIT_ATLAS_IWYU_PREFIX}")
file(MAKE_DIRECTORY "${work}/source")

execute_process(
    COMMAND "${edit_atlas_iwyu_git}" init --quiet
    WORKING_DIRECTORY "${work}/source"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${edit_atlas_iwyu_git}" fetch --quiet --depth 1
        https://github.com/include-what-you-use/include-what-you-use.git
        "${EDIT_ATLAS_IWYU_COMMIT}"
    WORKING_DIRECTORY "${work}/source"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${edit_atlas_iwyu_git}" checkout --quiet FETCH_HEAD
    WORKING_DIRECTORY "${work}/source"
    COMMAND_ERROR_IS_FATAL ANY
)

# IWYU links Clang's libraries, so it builds with the Clang it analyses for.
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${work}/source"
        -B "${work}/build"
        -G Ninja
        "-DCMAKE_MAKE_PROGRAM=${edit_atlas_iwyu_ninja}"
        -DCMAKE_BUILD_TYPE=Release
        "-DCMAKE_C_COMPILER=${EDIT_ATLAS_IWYU_LLVM_PREFIX}/bin/clang"
        "-DCMAKE_CXX_COMPILER=${EDIT_ATLAS_IWYU_LLVM_PREFIX}/bin/clang++"
        "-DCMAKE_PREFIX_PATH=${EDIT_ATLAS_IWYU_LLVM_PREFIX}"
        "-DCMAKE_INSTALL_PREFIX=${EDIT_ATLAS_IWYU_PREFIX}"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${work}/build"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${work}/build"
    COMMAND_ERROR_IS_FATAL ANY
)
# IWYU installs no mapping generators; the project generates its Qt mapping
# from the configured Qt with the one matching this release.
file(
    COPY "${work}/source/mapgen/iwyu-mapgen-qt.py"
    DESTINATION "${EDIT_ATLAS_IWYU_PREFIX}/share/include-what-you-use"
)
file(REMOVE_RECURSE "${work}")
