option(AQUILA_CLANG_TIDY "Run clang-tidy (.clang-tidy) on Aquila's own sources while they compile" OFF)
set(AQUILA_CHECK_BASE "develop" CACHE STRING "Branch the FormatDiff/TidyDiff targets compare against")

function(_aquila_collect_targets out dir)
    get_property(_targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
    get_property(_subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
    foreach (_subdir IN LISTS _subdirs)
        _aquila_collect_targets(_nested "${_subdir}")
        list(APPEND _targets ${_nested})
    endforeach ()
    set(${out} ${_targets} PARENT_SCOPE)
endfunction()

_aquila_collect_targets(_aquila_all_targets "${CMAKE_SOURCE_DIR}")

set(AQUILA_PROJECT_TARGETS "")
set(AQUILA_TEST_TARGETS "")
set(AQUILA_EXECUTABLE_TARGETS "")
foreach (_target IN LISTS _aquila_all_targets)
    get_target_property(_type ${_target} TYPE)
    if (NOT _type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
        continue()
    endif ()
    get_target_property(_dir ${_target} SOURCE_DIR)
    if (_dir MATCHES "^${CMAKE_SOURCE_DIR}/Engine/Vendor(/|$)")
        continue()
    endif ()
    if (_type STREQUAL "EXECUTABLE")
        list(APPEND AQUILA_EXECUTABLE_TARGETS ${_target})
    endif ()
    if (_dir MATCHES "^${CMAKE_SOURCE_DIR}/Engine/Tests(/|$)")
        if (_type STREQUAL "EXECUTABLE")
            list(APPEND AQUILA_TEST_TARGETS ${_target})
        endif ()
    else ()
        list(APPEND AQUILA_PROJECT_TARGETS ${_target})
    endif ()
endforeach ()

if (AQUILA_SANITIZERS AND AQUILA_EXECUTABLE_TARGETS)
    aquila_add_sanitizer_hooks(${AQUILA_EXECUTABLE_TARGETS})
endif ()

if (AQUILA_COVERAGE)
    aquila_add_coverage("${AQUILA_PROJECT_TARGETS}" "${AQUILA_TEST_TARGETS}" "${AQUILA_EXECUTABLE_TARGETS}")
endif ()

get_filename_component(_aquila_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(AQUILA_CLANG_TIDY_EXE clang-tidy HINTS "${_aquila_compiler_dir}")
find_program(AQUILA_CLANG_FORMAT_EXE clang-format HINTS "${_aquila_compiler_dir}")
find_program(AQUILA_GIT_CLANG_FORMAT_EXE git-clang-format HINTS "${_aquila_compiler_dir}")
find_package(Python3 COMPONENTS Interpreter QUIET)
find_file(AQUILA_CLANG_TIDY_DIFF clang-tidy-diff.py
    HINTS "${_aquila_compiler_dir}/../share/clang" /usr/share/clang /usr/lib/llvm/share/clang
)

if (AQUILA_CLANG_TIDY)
    if (NOT AQUILA_CLANG_TIDY_EXE)
        message(FATAL_ERROR "AQUILA_CLANG_TIDY is ON but clang-tidy was not found")
    endif ()
    set_target_properties(${AQUILA_PROJECT_TARGETS} PROPERTIES
        CXX_CLANG_TIDY "${AQUILA_CLANG_TIDY_EXE};--quiet"
    )
    message(STATUS "clang-tidy: ON while compiling (${AQUILA_CLANG_TIDY_EXE})")
endif ()

file(GLOB_RECURSE AQUILA_FORMAT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Engine/Include/*.h"
    "${CMAKE_SOURCE_DIR}/Engine/Source/*.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Tests/*.h"
    "${CMAKE_SOURCE_DIR}/Engine/Tests/*.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/Player/*.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/ShaderBaker/*.cpp"
    "${CMAKE_SOURCE_DIR}/Engine/main.cpp"
    "${CMAKE_SOURCE_DIR}/Editor/*.h"
    "${CMAKE_SOURCE_DIR}/Editor/*.cpp"
)

if (AQUILA_CLANG_FORMAT_EXE)
    add_custom_target(Format
        COMMAND "${AQUILA_CLANG_FORMAT_EXE}" -i ${AQUILA_FORMAT_SOURCES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "[AQUILA] clang-format on every Aquila source"
        VERBATIM
    )
    add_custom_target(FormatCheck
        COMMAND "${AQUILA_CLANG_FORMAT_EXE}" --dry-run --Werror ${AQUILA_FORMAT_SOURCES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "[AQUILA] Checking that every Aquila source is clang-formatted"
        VERBATIM
    )
endif ()

if (AQUILA_GIT_CLANG_FORMAT_EXE AND AQUILA_CLANG_FORMAT_EXE AND GIT_FOUND)
    add_custom_target(FormatDiff
        COMMAND "${CMAKE_COMMAND}"
                -DGIT=${GIT_EXECUTABLE}
                -DGIT_CLANG_FORMAT=${AQUILA_GIT_CLANG_FORMAT_EXE}
                -DCLANG_FORMAT=${AQUILA_CLANG_FORMAT_EXE}
                -DBASE=${AQUILA_CHECK_BASE}
                -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                -DMODE=format
                -P "${CMAKE_SOURCE_DIR}/cmake/AquilaDiffCheck.cmake"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "[AQUILA] Checking clang-format on lines changed since ${AQUILA_CHECK_BASE}"
        USES_TERMINAL
        VERBATIM
    )
endif ()

if (AQUILA_CLANG_TIDY_EXE AND AQUILA_CLANG_TIDY_DIFF AND Python3_Interpreter_FOUND AND GIT_FOUND)
    include(ProcessorCount)
    ProcessorCount(_aquila_jobs)
    add_custom_target(TidyDiff
        COMMAND "${CMAKE_COMMAND}"
                -DGIT=${GIT_EXECUTABLE}
                -DPYTHON=${Python3_EXECUTABLE}
                -DCLANG_TIDY=${AQUILA_CLANG_TIDY_EXE}
                -DCLANG_TIDY_DIFF=${AQUILA_CLANG_TIDY_DIFF}
                -DBASE=${AQUILA_CHECK_BASE}
                -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
                -DBINARY_DIR=${CMAKE_BINARY_DIR}
                -DJOBS=${_aquila_jobs}
                -DMODE=tidy
                -P "${CMAKE_SOURCE_DIR}/cmake/AquilaDiffCheck.cmake"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "[AQUILA] clang-tidy on lines changed since ${AQUILA_CHECK_BASE}"
        USES_TERMINAL
        VERBATIM
    )
endif ()
