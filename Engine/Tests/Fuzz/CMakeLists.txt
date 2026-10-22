set(AQUILA_FUZZ_SECONDS 60 CACHE STRING "How long each fuzzer runs in the Fuzz target")

add_custom_target(Fuzzers)
add_custom_target(Fuzz COMMENT "[AQUILA] Fuzzing, ${AQUILA_FUZZ_SECONDS} s per fuzzer")

function(aquila_add_fuzzer name)
    cmake_parse_arguments(PARSE_ARGV 1 FUZZ "" "" "SEEDS")

    add_executable(${name} ${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp)
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/Engine/Include)
    target_precompile_headers(${name} PRIVATE ${CMAKE_SOURCE_DIR}/Engine/Include/BasePCH.h)
    target_link_libraries(${name} PRIVATE Aquila::Engine)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    add_dependencies(Fuzzers ${name})

    set(_corpus "${CMAKE_CURRENT_SOURCE_DIR}/Corpus/${name}")
    set(_seeds ${FUZZ_SEEDS})
    if (IS_DIRECTORY "${_corpus}")
        list(PREPEND _seeds "${_corpus}")
    endif ()

    add_test(NAME ${name} COMMAND ${name} -runs=0 ${_seeds})

    set(_work "${CMAKE_CURRENT_BINARY_DIR}/Runs/${name}")
    add_custom_target(Fuzz-${name}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_work}/corpus" "${_work}/crashes"
        COMMAND ${name} -max_total_time=${AQUILA_FUZZ_SECONDS} -print_final_stats=1
                -artifact_prefix=${_work}/crashes/ "${_work}/corpus" ${_seeds}
        DEPENDS ${name}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "[AQUILA] Fuzzing ${name} for ${AQUILA_FUZZ_SECONDS} s, crashes go to ${_work}/crashes"
        USES_TERMINAL
        VERBATIM
    )
    add_dependencies(Fuzz Fuzz-${name})
endfunction()

aquila_add_fuzzer(StyleParserFuzzer SEEDS ${CMAKE_SOURCE_DIR}/Resources/Engine/UI)
aquila_add_fuzzer(LayoutLoaderFuzzer SEEDS ${CMAKE_SOURCE_DIR}/Resources/Engine/UI)
