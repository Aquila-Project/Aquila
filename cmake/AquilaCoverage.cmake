option(AQUILA_COVERAGE "Instrument Aquila's own code for clang source-based coverage (llvm-cov)" OFF)

if (NOT AQUILA_COVERAGE)
    return()
endif ()

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR AQUILA_CLANG_CL)
    message(FATAL_ERROR "AQUILA_COVERAGE needs clang with the GNU driver")
endif ()

get_filename_component(_aquila_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(AQUILA_LLVM_PROFDATA llvm-profdata HINTS "${_aquila_compiler_dir}")
find_program(AQUILA_LLVM_COV llvm-cov HINTS "${_aquila_compiler_dir}")
if (NOT AQUILA_LLVM_PROFDATA OR NOT AQUILA_LLVM_COV)
    message(FATAL_ERROR "AQUILA_COVERAGE needs llvm-profdata and llvm-cov from the same LLVM as ${CMAKE_CXX_COMPILER}")
endif ()

set(AQUILA_COVERAGE_DIR "${CMAKE_BINARY_DIR}/Coverage")
set(AQUILA_COVERAGE_RAW_DIR "${AQUILA_COVERAGE_DIR}/raw")
set(AQUILA_COVERAGE_FLAGS -fprofile-instr-generate=${AQUILA_COVERAGE_RAW_DIR}/%m.profraw -fcoverage-mapping)
add_link_options(-fprofile-instr-generate)

function(aquila_add_coverage project_targets test_targets executables)
    foreach (_target IN LISTS project_targets test_targets)
        target_compile_options(${_target} PRIVATE ${AQUILA_COVERAGE_FLAGS})
    endforeach ()

    set(_tests "")
    foreach (_target IN LISTS test_targets)
        string(APPEND _tests "$<TARGET_FILE:${_target}>\n")
    endforeach ()
    set(_objects "")
    foreach (_target IN LISTS executables)
        string(APPEND _objects "$<TARGET_FILE:${_target}>\n")
    endforeach ()
    file(GENERATE OUTPUT "${AQUILA_COVERAGE_DIR}/tests.txt" CONTENT "${_tests}")
    file(GENERATE OUTPUT "${AQUILA_COVERAGE_DIR}/objects.txt" CONTENT "${_objects}")

    set(_script_args
        -DRAW_DIR=${AQUILA_COVERAGE_RAW_DIR}
        -DOUT_DIR=${AQUILA_COVERAGE_DIR}
        -DPROFDATA=${AQUILA_LLVM_PROFDATA}
        -DLLVM_COV=${AQUILA_LLVM_COV}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_BINARY_DIR}
    )

    add_custom_target(Coverage
        COMMAND ${CMAKE_COMMAND} ${_script_args} -DRUN_TESTS=ON -P "${CMAKE_SOURCE_DIR}/cmake/AquilaCoverageReport.cmake"
        DEPENDS ${test_targets} ${executables}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "[AQUILA] Running the tests and writing the coverage report"
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(CoverageReport
        COMMAND ${CMAKE_COMMAND} ${_script_args} -DRUN_TESTS=OFF -P "${CMAKE_SOURCE_DIR}/cmake/AquilaCoverageReport.cmake"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "[AQUILA] Writing the coverage report from the profiles already recorded"
        USES_TERMINAL
        VERBATIM
    )
endfunction()

message(STATUS "Coverage: ON (${AQUILA_COVERAGE_DIR})")
