# Engine/CMakeLists.txt

set(ENGINE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(VENDOR_DIR ${ENGINE_ROOT_DIR}/Vendor)
set(ENGINE_INCLUDE_DIR ${ENGINE_ROOT_DIR}/Include)

set(STB_INCLUDE_DIR ${VENDOR_DIR}/stb)

set(GLM_INCLUDE_DIRS ${VENDOR_DIR}/glm)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if (UNIX AND NOT APPLE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif ()
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
add_subdirectory(${VENDOR_DIR}/glfw)

target_compile_definitions(glfw INTERFACE GLFW_INCLUDE_NONE)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_FBX_IMPORTER ON)
set(ASSIMP_BUILD_GLTF_IMPORTER ON)
set(ASSIMP_BUILD_OBJ_IMPORTER ON)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON)
set(ASSIMP_BUILD_DOCS OFF)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

if(UNIX)
    set(ASSIMP_BUILD_MINIZIP ON CACHE BOOL "" FORCE)
endif()

set(ASSIMP_BUILD_USE_CCACHE OFF CACHE BOOL "" FORCE)

add_subdirectory(${VENDOR_DIR}/assimp SYSTEM)
if (TARGET zlibstatic)
    target_compile_options(zlibstatic PRIVATE -w)
endif ()
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(assimp PRIVATE
        -Wno-padded
        -Wno-unsafe-buffer-usage
        -Wno-unsafe-buffer-usage-in-libc-call
        -Wno-switch-default
        -Wno-nrvo
        -Wno-unique-object-duplication
    )
endif()


set(VMA_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
set(VMA_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(VMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${VENDOR_DIR}/VulkanMemoryAllocator)

enable_language(C)
add_subdirectory(${VENDOR_DIR}/hosek)

set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS ON CACHE BOOL "" FORCE)
set(USE_SYSTEM_PLUTOVG OFF CACHE BOOL "" FORCE)
add_subdirectory(${VENDOR_DIR}/lunasvg)

add_subdirectory(${VENDOR_DIR}/aquiLua)


add_subdirectory(Source/Aquila/Foundation)
add_subdirectory(Source/Aquila/Platform)
add_subdirectory(Source/Aquila/Application)
add_subdirectory(Source/Aquila/RHI)
add_subdirectory(Source/Aquila/GFX)
add_subdirectory(Source/Aquila/Graphics)
add_subdirectory(Source/Aquila/Scene)
add_subdirectory(Source/Aquila/Rendering)
add_subdirectory(Source/Aquila/UI)
add_subdirectory(Source/Aquila/Scripting)

enable_testing()
add_subdirectory(Tests)

set(AQUILA_ENGINE_MODULES Foundation Platform RHI GFX Graphics Scene Rendering Application UI Scripting)

if (AQUILA_SHARED_ENGINE)
    set(AQUILA_RUNTIME_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/AquilaRuntime.cpp)
    file(CONFIGURE OUTPUT ${AQUILA_RUNTIME_SOURCE} CONTENT "")
    add_library(AquilaRuntime SHARED ${AQUILA_RUNTIME_SOURCE})
    set_target_properties(AquilaRuntime PROPERTIES
        OUTPUT_NAME "Aquila"
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    target_link_libraries(AquilaRuntime
        PUBLIC "$<COMPILE_ONLY:${AQUILA_ENGINE_MODULES}>"
        PRIVATE "$<LINK_LIBRARY:WHOLE_ARCHIVE,${AQUILA_ENGINE_MODULES}>"
    )
else ()
    add_library(AquilaRuntime INTERFACE)
    target_link_libraries(AquilaRuntime INTERFACE ${AQUILA_ENGINE_MODULES})
endif ()
add_library(Aquila::Engine ALIAS AquilaRuntime)

function(aquila_link_engine target)
    target_link_libraries(${target} PRIVATE Aquila::Engine)
    aquila_deploy_runtime(${target})
    if (AQUILA_SHARED_ENGINE)
        set_target_properties(${target} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    endif ()
endfunction()

add_executable(AquilaEngine
    ${ENGINE_ROOT_DIR}/main.cpp
)
target_precompile_headers(AquilaEngine PRIVATE
    ${ENGINE_INCLUDE_DIR}/GraphicsPCH.h
)
aquila_link_engine(AquilaEngine)
