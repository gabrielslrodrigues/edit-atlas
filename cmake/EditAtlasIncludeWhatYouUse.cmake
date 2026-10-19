include_guard(GLOBAL)

# Each include-what-you-use release builds against exactly one Clang major
# (its clang_<major> branch), so the release follows the project's compiler.
# Extend this table alongside the Clang versions LinuxClangToolchain.cmake
# accepts.
set(_edit_atlas_iwyu_clang_19 0.23 30e221fad0967aa6bcb279600127748ed3dab7ad)
set(_edit_atlas_iwyu_clang_20 0.24 6e08906c66b3009f2d590e4bd40d60fa303bf803)
set(_edit_atlas_iwyu_clang_21 0.25 791e69ea4662cb3e74e8128fd5fd69bd7f4ea6b3)
set(_edit_atlas_iwyu_clang_22 0.26 01a091d16b3dedb808db21f32ed3e761737a3691)

# Runs include-what-you-use alongside every C++ compilation and fails the
# build when it reports a change, building the matching release on first use.
# Call before creating targets: they copy the settings when created.
macro(edit_atlas_enable_include_what_you_use)
    _edit_atlas_include_what_you_use_command(
        _edit_atlas_include_what_you_use
    )
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${_edit_atlas_include_what_you_use})
    # IWYU cannot read precompiled headers, and a forced PCH include would
    # hide the includes it checks.
    set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
    cmake_language(
        DEFER
        DIRECTORY "${PROJECT_SOURCE_DIR}"
        CALL _edit_atlas_skip_generated_sources "${PROJECT_SOURCE_DIR}"
    )
endmacro()

# Excludes generated sources, such as Qt's resource initializers, whose
# includes the project does not control. CMake already skips its own AUTOGEN
# output. Qt writes some sources at configure time without marking them
# GENERATED, so any source in the build tree counts as generated.
function(_edit_atlas_skip_generated_sources directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(sources "${target}" SOURCES)
        if(NOT sources)
            continue()
        endif()
        get_target_property(source_directory "${target}" SOURCE_DIR)
        foreach(source IN LISTS sources)
            if(source MATCHES "^\\$<")
                continue()
            endif()
            get_source_file_property(
                generated "${source}" TARGET_DIRECTORY "${target}" GENERATED
            )
            cmake_path(
                ABSOLUTE_PATH source
                BASE_DIRECTORY "${source_directory}"
                OUTPUT_VARIABLE path
            )
            cmake_path(IS_PREFIX PROJECT_BINARY_DIR "${path}" in_build_tree)
            if(generated OR in_build_tree)
                set_source_files_properties(
                    "${source}"
                    TARGET_DIRECTORY "${target}"
                    PROPERTIES SKIP_LINTING ON
                )
            endif()
        endforeach()
    endforeach()
    get_property(
        subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES
    )
    foreach(subdirectory IN LISTS subdirectories)
        _edit_atlas_skip_generated_sources("${subdirectory}")
    endforeach()
endfunction()

function(_edit_atlas_include_what_you_use_command output)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(
            FATAL_ERROR
            "EDIT_ATLAS_INCLUDE_WHAT_YOU_USE requires Clang; the configured "
            "compiler is ${CMAKE_CXX_COMPILER_ID}."
        )
    endif()
    if(NOT CMAKE_GENERATOR MATCHES "^(Ninja|.*Makefiles)")
        message(
            FATAL_ERROR
            "EDIT_ATLAS_INCLUDE_WHAT_YOU_USE requires a Ninja or Makefile "
            "generator; CMake runs no include-what-you-use for "
            "${CMAKE_GENERATOR}."
        )
    endif()
    string(REGEX MATCH "^[0-9]+" major "${CMAKE_CXX_COMPILER_VERSION}")
    if(NOT DEFINED "_edit_atlas_iwyu_clang_${major}")
        message(
            FATAL_ERROR
            "No include-what-you-use release is pinned for Clang ${major}; "
            "add one to EditAtlasIncludeWhatYouUse.cmake."
        )
    endif()
    list(GET "_edit_atlas_iwyu_clang_${major}" 0 release)
    list(GET "_edit_atlas_iwyu_clang_${major}" 1 commit)

    # A versioned driver is a link into its LLVM installation, so the
    # resolved driver locates the Clang libraries IWYU builds against.
    file(REAL_PATH "${CMAKE_CXX_COMPILER}" driver)
    cmake_path(GET driver PARENT_PATH llvm_prefix)
    cmake_path(GET llvm_prefix PARENT_PATH llvm_prefix)
    if(NOT EXISTS "${llvm_prefix}/lib/cmake/clang/ClangConfig.cmake")
        message(
            FATAL_ERROR
            "Clang ${major}'s development files were not found under "
            "${llvm_prefix}. Install them (on Ubuntu, llvm-${major}-dev and "
            "libclang-${major}-dev) to build include-what-you-use."
        )
    endif()

    set(
        EDIT_ATLAS_INCLUDE_WHAT_YOU_USE_DIR
        "${PROJECT_SOURCE_DIR}/build/include-what-you-use"
        CACHE PATH
        "Directory holding include-what-you-use builds, one per release"
    )
    set(prefix "${EDIT_ATLAS_INCLUDE_WHAT_YOU_USE_DIR}/${release}-clang${major}")
    set(executable "${prefix}/bin/include-what-you-use")
    set(qt_generator "${prefix}/share/include-what-you-use/iwyu-mapgen-qt.py")
    # The generator is installed last, so its presence marks a finished build.
    if(NOT EXISTS "${qt_generator}")
        message(
            STATUS
            "Building include-what-you-use ${release} for Clang ${major}"
        )
        execute_process(
            COMMAND "${CMAKE_COMMAND}"
                "-DEDIT_ATLAS_IWYU_COMMIT=${commit}"
                "-DEDIT_ATLAS_IWYU_LLVM_PREFIX=${llvm_prefix}"
                "-DEDIT_ATLAS_IWYU_PREFIX=${prefix}"
                -P "${PROJECT_SOURCE_DIR}/cmake/BuildIncludeWhatYouUse.cmake"
            COMMAND_ERROR_IS_FATAL ANY
        )
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(qt_headers "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_HEADERS}")
    set(qt_mapping "${PROJECT_BINARY_DIR}/include-what-you-use/qt6.imp")
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/include-what-you-use")
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${qt_generator}" "${qt_headers}"
        OUTPUT_FILE "${qt_mapping}"
        COMMAND_ERROR_IS_FATAL ANY
    )

    # IWYU uses the builtin headers of the Clang it was built from, which an
    # installed IWYU cannot locate on its own.
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir
        OUTPUT_VARIABLE resource_directory
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )

    set(
        "${output}"
        "${executable}"
        -Xiwyu --error
        -Xiwyu --no_fwd_decls
        -Xiwyu --cxx17ns
        "-Xiwyu" "--mapping_file=${qt_mapping}"
        "-Xiwyu"
        "--mapping_file=${PROJECT_SOURCE_DIR}/tools/iwyu/edit_atlas.imp"
        "-resource-dir=${resource_directory}"
        PARENT_SCOPE
    )
endfunction()
