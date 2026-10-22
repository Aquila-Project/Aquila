set(AQUILA_SANITIZERS "" CACHE STRING "Sanitizers for every C/C++ target, a list of address, undefined, leak, thread")
option(AQUILA_FUZZ "Build the libFuzzer targets in Engine/Tests/Fuzz (clang, needs AQUILA_SANITIZERS=address)" OFF)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_aquila_hardening_default ON)
else ()
    set(_aquila_hardening_default OFF)
endif ()
option(AQUILA_HARDENING "Standard library precondition checks and pattern-initialized locals" ${_aquila_hardening_default})

set(AQUILA_SANITIZER_DIR "${CMAKE_SOURCE_DIR}/Tools/Sanitizers")

if (AQUILA_HARDENING AND NOT AQUILA_CLANG_CL)
    add_compile_definitions(
        _GLIBCXX_ASSERTIONS
        _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE
    )
    add_compile_options(-ftrivial-auto-var-init=pattern)
    message(STATUS "Hardening: ON (stdlib assertions, pattern-initialized locals)")
else ()
    message(STATUS "Hardening: OFF")
endif ()

if (NOT AQUILA_SANITIZERS AND NOT AQUILA_FUZZ)
    message(STATUS "Sanitizers: OFF")
    return()
endif ()

if (AQUILA_FUZZ)
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "AQUILA_FUZZ needs clang (libFuzzer)")
    endif ()
    if (NOT "address" IN_LIST AQUILA_SANITIZERS)
        list(APPEND AQUILA_SANITIZERS address)
    endif ()
endif ()

if (AQUILA_CLANG_CL OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    message(FATAL_ERROR "AQUILA_SANITIZERS is only set up for clang or gcc on Linux")
endif ()

foreach (_sanitizer IN LISTS AQUILA_SANITIZERS)
    if (NOT _sanitizer MATCHES "^(address|undefined|leak|thread)$")
        message(FATAL_ERROR "Unknown sanitizer '${_sanitizer}' in AQUILA_SANITIZERS (address, undefined, leak, thread)")
    endif ()
endforeach ()

if ("thread" IN_LIST AQUILA_SANITIZERS AND ("address" IN_LIST AQUILA_SANITIZERS OR "leak" IN_LIST AQUILA_SANITIZERS))
    message(FATAL_ERROR "The thread sanitizer can't be combined with address or leak")
endif ()

list(JOIN AQUILA_SANITIZERS "," _aquila_sanitize)
add_compile_options(-fsanitize=${_aquila_sanitize} -fno-omit-frame-pointer -fno-optimize-sibling-calls)
add_link_options(-fsanitize=${_aquila_sanitize})

if ("address" IN_LIST AQUILA_SANITIZERS)
    add_compile_definitions(_GLIBCXX_SANITIZE_VECTOR)
endif ()

if (AQUILA_FUZZ)
    add_compile_options(-fsanitize=fuzzer-no-link)
    add_compile_definitions(AQUILA_FUZZING)
endif ()

function(_aquila_sanitizer_options out name)
    set(_options ${ARGN})
    if (EXISTS "${AQUILA_SANITIZER_DIR}/${name}.supp")
        list(APPEND _options "suppressions='${AQUILA_SANITIZER_DIR}/${name}.supp'")
    endif ()
    list(JOIN _options ":" _joined)
    set(${out} "${_joined}" PARENT_SCOPE)
endfunction()

_aquila_sanitizer_options(AQUILA_ASAN_OPTIONS asan
    detect_stack_use_after_return=1
    check_initialization_order=1
    detect_leaks=1
    print_suppressions=0
)
_aquila_sanitizer_options(AQUILA_LSAN_OPTIONS lsan print_suppressions=0)
_aquila_sanitizer_options(AQUILA_UBSAN_OPTIONS ubsan print_stacktrace=1 halt_on_error=1 print_suppressions=0)
_aquila_sanitizer_options(AQUILA_TSAN_OPTIONS tsan second_deadlock_stack=1 history_size=4 print_suppressions=0)

set(AQUILA_SANITIZER_HOOKS "${CMAKE_BINARY_DIR}/Sanitizers/SanitizerHooks.cpp")
configure_file("${AQUILA_SANITIZER_DIR}/SanitizerHooks.cpp.in" "${AQUILA_SANITIZER_HOOKS}" @ONLY)

function(aquila_add_sanitizer_hooks)
    foreach (_target IN LISTS ARGN)
        target_sources(${_target} PRIVATE "${AQUILA_SANITIZER_HOOKS}")
    endforeach ()
    set_source_files_properties("${AQUILA_SANITIZER_HOOKS}" TARGET_DIRECTORY ${ARGN}
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON SKIP_LINTING ON
    )
endfunction()

message(STATUS "Sanitizers: ${_aquila_sanitize}")
