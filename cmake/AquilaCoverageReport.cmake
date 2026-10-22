file(STRINGS "${OUT_DIR}/tests.txt" _tests)
file(STRINGS "${OUT_DIR}/objects.txt" _objects)

if (RUN_TESTS)
    file(REMOVE_RECURSE "${RAW_DIR}")
    file(MAKE_DIRECTORY "${RAW_DIR}")
    set(_failed "")
    foreach (_test IN LISTS _tests)
        get_filename_component(_name "${_test}" NAME)
        message(STATUS "[AQUILA] Coverage: running ${_name}")
        get_filename_component(_dir "${_test}" DIRECTORY)
        execute_process(COMMAND "${_test}" --no-intro --minimal WORKING_DIRECTORY "${_dir}" RESULT_VARIABLE _result)
        if (NOT _result EQUAL 0)
            list(APPEND _failed "${_name}")
        endif ()
    endforeach ()
    if (_failed)
        message(WARNING "[AQUILA] Coverage: failing tests (the report still counts them): ${_failed}")
    endif ()
endif ()

file(GLOB _raw "${RAW_DIR}/*.profraw")
if (NOT _raw)
    message(FATAL_ERROR "[AQUILA] Coverage: no profiles in ${RAW_DIR}, run the tests or the editor first")
endif ()

set(_profdata "${OUT_DIR}/aquila.profdata")
execute_process(COMMAND "${PROFDATA}" merge -sparse ${_raw} -o "${_profdata}" COMMAND_ERROR_IS_FATAL ANY)

set(_object_args "")
foreach (_object IN LISTS _objects)
    if (EXISTS "${_object}")
        if (_object_args)
            list(APPEND _object_args -object "${_object}")
        else ()
            list(APPEND _object_args "${_object}")
        endif ()
    endif ()
endforeach ()

string(REPLACE "." "\\." _binary_regex "${BINARY_DIR}")
set(_ignore "-ignore-filename-regex=(/Engine/Vendor/|/Engine/Tests/|${_binary_regex}/)")

execute_process(
    COMMAND "${LLVM_COV}" show ${_object_args} -instr-profile=${_profdata} ${_ignore}
            -format=html -show-line-counts-or-regions -show-branches=count
            -project-title=Aquila -output-dir=${OUT_DIR}/html
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${LLVM_COV}" export ${_object_args} -instr-profile=${_profdata} ${_ignore} -format=lcov
    OUTPUT_FILE "${OUT_DIR}/lcov.info"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
    COMMAND "${LLVM_COV}" report ${_object_args} -instr-profile=${_profdata} ${_ignore} -summary-only
    OUTPUT_FILE "${OUT_DIR}/summary.txt"
    COMMAND_ERROR_IS_FATAL ANY
)
execute_process(COMMAND "${LLVM_COV}" report ${_object_args} -instr-profile=${_profdata} ${_ignore})

message(STATUS "[AQUILA] Coverage: ${OUT_DIR}/html/index.html (lcov: ${OUT_DIR}/lcov.info)")
