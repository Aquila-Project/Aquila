option(AQUILA_LINT_UI "Lint .aqlayout and .aqstyle files with Tools/aqlint as part of the build" ON)

if (NOT AQUILA_LINT_UI)
    return()
endif ()

set(AQLINT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Tools/aqlint")
if (NOT EXISTS "${AQLINT_SOURCE_DIR}/Cargo.toml")
    message(STATUS "UI lint: OFF (Tools/aqlint not found)")
    return()
endif ()

find_program(CARGO_EXECUTABLE cargo HINTS "$ENV{HOME}/.cargo/bin" "$ENV{USERPROFILE}/.cargo/bin")
if (NOT CARGO_EXECUTABLE)
    message(STATUS "UI lint: OFF (cargo not found, install Rust to enable it)")
    return()
endif ()

file(GLOB_RECURSE AQUILA_UI_MARKUP CONFIGURE_DEPENDS
    "${AQUILA_RESOURCES_PATH}/*.aqlayout" "${AQUILA_RESOURCES_PATH}/*.aqstyle"
    "${AQUILA_ASSET_PATH}/*.aqlayout" "${AQUILA_ASSET_PATH}/*.aqstyle"
)
if (NOT AQUILA_UI_MARKUP)
    message(STATUS "UI lint: OFF (no .aqlayout or .aqstyle files)")
    return()
endif ()

set(AQLINT_ROOTS "")
foreach (_root "${AQUILA_RESOURCES_PATH}" "${AQUILA_ASSET_PATH}")
    if (IS_DIRECTORY "${_root}")
        list(APPEND AQLINT_ROOTS "${_root}")
    endif ()
endforeach ()

set(AQLINT_BINARY_DIR "${CMAKE_BINARY_DIR}/aqlint")
set(AQLINT_EXECUTABLE "${AQLINT_BINARY_DIR}/release/aqlint${CMAKE_EXECUTABLE_SUFFIX}")
set(AQLINT_STAMP "${AQLINT_BINARY_DIR}/lint.stamp")

file(GLOB_RECURSE AQLINT_SOURCES CONFIGURE_DEPENDS "${AQLINT_SOURCE_DIR}/src/*.rs")

add_custom_command(
    OUTPUT "${AQLINT_EXECUTABLE}"
    COMMAND "${CARGO_EXECUTABLE}" build --release --quiet
            --manifest-path "${AQLINT_SOURCE_DIR}/Cargo.toml"
            --target-dir "${AQLINT_BINARY_DIR}"
    DEPENDS ${AQLINT_SOURCES} "${AQLINT_SOURCE_DIR}/Cargo.toml" "${AQLINT_SOURCE_DIR}/Cargo.lock"
    COMMENT "[AQUILA] Building aqlint"
    VERBATIM
)

add_custom_command(
    OUTPUT "${AQLINT_STAMP}"
    COMMAND "${AQLINT_EXECUTABLE}" ${AQLINT_ROOTS}
    COMMAND "${CMAKE_COMMAND}" -E touch "${AQLINT_STAMP}"
    DEPENDS "${AQLINT_EXECUTABLE}" ${AQUILA_UI_MARKUP}
    COMMENT "[AQUILA] Linting UI layouts and styles"
    VERBATIM
)

add_custom_target(LintUI ALL DEPENDS "${AQLINT_STAMP}")

foreach (_target AquilaEngine AquilaEditor)
    if (TARGET ${_target})
        add_dependencies(LintUI ${_target})
    endif ()
endforeach ()

list(LENGTH AQUILA_UI_MARKUP _markup_count)
message(STATUS "UI lint: ON (${_markup_count} files, cargo at ${CARGO_EXECUTABLE})")
