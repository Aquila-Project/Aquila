set(AQUILA_VULKAN_VERSION 1.4.357)

option(AQUILA_FETCH_VULKAN "Build the Vulkan headers and loader from source instead of using a system or SDK install" OFF)

function(aquila_detect_vulkan)
    set(AQUILA_BUILD_VULKAN_LOADER OFF PARENT_SCOPE)
    if (NOT AQUILA_FETCH_VULKAN)
        find_package(Vulkan QUIET)
        if (Vulkan_FOUND)
            return()
        endif ()
    endif ()
    set(AQUILA_BUILD_VULKAN_LOADER ON PARENT_SCOPE)
endfunction()

macro(aquila_provide_vulkan)
    if (AQUILA_BUILD_VULKAN_LOADER)
        message(STATUS "Aquila: building Vulkan headers and loader ${AQUILA_VULKAN_VERSION} from source")
        include(FetchContent)
        FetchContent_Declare(aquila_vulkan_headers
            URL https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v${AQUILA_VULKAN_VERSION}.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP ON
        )
        FetchContent_MakeAvailable(aquila_vulkan_headers)

        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_WSI_WAYLAND_SUPPORT OFF CACHE BOOL "" FORCE)
        set(BUILD_WSI_DIRECTFB_SUPPORT OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(aquila_vulkan_loader
            URL https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/v${AQUILA_VULKAN_VERSION}.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP ON
        )
        FetchContent_MakeAvailable(aquila_vulkan_loader)

        add_library(AquilaVulkan INTERFACE)
        target_link_libraries(AquilaVulkan INTERFACE vulkan Vulkan::Headers)
        add_library(Vulkan::Vulkan ALIAS AquilaVulkan)
        set(Vulkan_FOUND TRUE)
        set(Vulkan_INCLUDE_DIRS "${aquila_vulkan_headers_SOURCE_DIR}/include")
    else ()
        find_package(Vulkan REQUIRED)
    endif ()
endmacro()

function(aquila_detect_validation_layers out_var)
    set(_dirs "")
    foreach (_variable VK_LAYER_PATH VK_ADD_LAYER_PATH)
        if (DEFINED ENV{${_variable}})
            set(_entries "$ENV{${_variable}}")
            if (NOT WIN32)
                string(REPLACE ":" ";" _entries "${_entries}")
            endif ()
            list(APPEND _dirs ${_entries})
        endif ()
    endforeach ()
    if (WIN32)
        if (VULKAN_SDK)
            list(APPEND _dirs "${VULKAN_SDK}/Bin")
        endif ()
    else ()
        if (DEFINED ENV{XDG_DATA_HOME})
            list(APPEND _dirs "$ENV{XDG_DATA_HOME}/vulkan/explicit_layer.d")
        endif ()
        list(APPEND _dirs
            "$ENV{HOME}/.local/share/vulkan/explicit_layer.d"
            /usr/local/share/vulkan/explicit_layer.d
            /usr/share/vulkan/explicit_layer.d
            /usr/local/etc/vulkan/explicit_layer.d
            /etc/vulkan/explicit_layer.d
        )
    endif ()

    set(_found OFF)
    foreach (_dir IN LISTS _dirs)
        if (EXISTS "${_dir}/VkLayer_khronos_validation.json")
            set(_found ON)
            break()
        endif ()
    endforeach ()
    set(${out_var} ${_found} PARENT_SCOPE)
endfunction()
