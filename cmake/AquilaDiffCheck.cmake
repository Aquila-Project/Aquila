execute_process(
    COMMAND "${GIT}" merge-base "${BASE}" HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    OUTPUT_VARIABLE _base
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _result
)
if (NOT _result EQUAL 0)
    message(FATAL_ERROR "[AQUILA] '${BASE}' is not a known branch or commit (set AQUILA_CHECK_BASE)")
endif ()

set(_paths -- Engine/Include Engine/Source Engine/Tests Engine/Player Engine/main.cpp Editor)

if (MODE STREQUAL "format")
    execute_process(
        COMMAND "${GIT_CLANG_FORMAT}" --binary "${CLANG_FORMAT}" --diff --extensions h,hpp,cpp "${_base}" ${_paths}
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE _diff
        RESULT_VARIABLE _result
    )
    if (_diff MATCHES "^diff " OR _diff MATCHES "\ndiff ")
        message("${_diff}")
        message(FATAL_ERROR "[AQUILA] Changed lines are not clang-formatted, run: git clang-format ${_base}")
    endif ()
    if (NOT _result EQUAL 0 AND NOT _result EQUAL 1)
        message(FATAL_ERROR "[AQUILA] git clang-format failed (${_result})")
    endif ()
    message(STATUS "[AQUILA] Changed lines since ${BASE} are clang-formatted")
elseif (MODE STREQUAL "tidy")
    execute_process(
        COMMAND "${GIT}" diff -U0 --no-color "${_base}" ${_paths}
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_FILE "${BINARY_DIR}/tidy-diff.patch"
    )
    file(SIZE "${BINARY_DIR}/tidy-diff.patch" _size)
    if (_size EQUAL 0)
        message(STATUS "[AQUILA] No changed sources since ${BASE}")
        return()
    endif ()
    execute_process(
        COMMAND "${PYTHON}" "${CLANG_TIDY_DIFF}" -p1 -path "${BINARY_DIR}" -clang-tidy-binary "${CLANG_TIDY}"
                -j ${JOBS} -quiet -only-check-in-db -iregex ".*\\.cpp"
        INPUT_FILE "${BINARY_DIR}/tidy-diff.patch"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE _result
    )
    if (NOT _result EQUAL 0)
        message(FATAL_ERROR "[AQUILA] clang-tidy reported errors on changed lines")
    endif ()
else ()
    message(FATAL_ERROR "MODE must be format or tidy")
endif ()
