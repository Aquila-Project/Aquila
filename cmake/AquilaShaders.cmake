include_guard(GLOBAL)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(_aquila_precompiled_shaders_default ON)
else ()
    set(_aquila_precompiled_shaders_default OFF)
endif ()
option(AQUILA_PRECOMPILED_SHADERS
    "Bake the engine shaders at build time and run and ship them without their source (Release and Shipping)"
    ${_aquila_precompiled_shaders_default})

set(AQUILA_ENGINE_SHADERS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Engine/Shaders")
set(AQUILA_SHADER_BAKE_SKIP "GBuffer.slang" CACHE STRING
    "Engine shaders the baker leaves out, relative to Engine/Shaders (dead ones that no longer compile)")
if (AQUILA_PRECOMPILED_SHADERS)
    set(AQUILA_ENGINE_SHADERS_DIR "${CMAKE_BINARY_DIR}/Shaders")
else ()
    set(AQUILA_ENGINE_SHADERS_DIR "${AQUILA_ENGINE_SHADERS_SOURCE_DIR}")
endif ()
message(STATUS "Precompiled shaders: ${AQUILA_PRECOMPILED_SHADERS} (engine shaders from ${AQUILA_ENGINE_SHADERS_DIR})")

function(aquila_add_shader_baker)
    if (NOT AQUILA_PRECOMPILED_SHADERS)
        return()
    endif ()

    add_executable(AquilaShaderBaker "${CMAKE_SOURCE_DIR}/Engine/ShaderBaker/main.cpp")
    target_link_libraries(AquilaShaderBaker PRIVATE Graphics)
    target_precompile_headers(AquilaShaderBaker PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Include/GraphicsPCH.h")
    aquila_deploy_runtime(AquilaShaderBaker)

    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${AQUILA_ENGINE_SHADERS_SOURCE_DIR}/*.slang")
    set(_pack "${AQUILA_ENGINE_SHADERS_DIR}/Engine.aqshaders")
    add_custom_command(
        OUTPUT "${_pack}"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${AQUILA_ENGINE_SHADERS_DIR}"
        COMMAND AquilaShaderBaker "${AQUILA_ENGINE_SHADERS_SOURCE_DIR}" "${AQUILA_ENGINE_SHADERS_DIR}"
                ${AQUILA_SHADER_BAKE_SKIP}
        DEPENDS AquilaShaderBaker ${_sources}
        COMMENT "[AQUILA] Baking the engine shaders"
        VERBATIM
    )
    add_custom_target(EngineShaders ALL DEPENDS "${_pack}")
endfunction()

function(aquila_depend_on_engine_shaders)
    if (NOT TARGET EngineShaders)
        return()
    endif ()
    foreach (_target IN LISTS ARGN)
        if (TARGET ${_target})
            add_dependencies(${_target} EngineShaders)
        endif ()
    endforeach ()
endfunction()
