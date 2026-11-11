option(AQUILA_SYSTEM_VULKAN "Use an installed Vulkan SDK or system loader instead of the Vulkan-Headers and Vulkan-Loader submodules" OFF)

set(AQUILA_VULKAN_HEADERS_DIR "${CMAKE_SOURCE_DIR}/Engine/Vendor/Vulkan-Headers")
set(AQUILA_VULKAN_LOADER_DIR "${CMAKE_SOURCE_DIR}/Engine/Vendor/Vulkan-Loader")

function(_aquila_expose_vulkan_headers_package)
    get_directory_property(_version DIRECTORY "${AQUILA_VULKAN_HEADERS_DIR}" DEFINITION VULKAN_HEADERS_VERSION)
    include(CMakePackageConfigHelpers)
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/vulkanheaders-config.cmake" "")
    write_basic_package_version_file("${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/vulkanheaders-config-version.cmake"
        VERSION ${_version}
        COMPATIBILITY SameMajorVersion
        ARCH_INDEPENDENT
    )
    message(STATUS "Aquila: building the Vulkan loader ${_version} from Engine/Vendor")
endfunction()

macro(aquila_provide_vulkan)
    if (AQUILA_SYSTEM_VULKAN)
        find_package(Vulkan REQUIRED)
    else ()
        add_subdirectory("${AQUILA_VULKAN_HEADERS_DIR}" SYSTEM)
        _aquila_expose_vulkan_headers_package()

        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_WSI_WAYLAND_SUPPORT OFF CACHE BOOL "" FORCE)
        set(BUILD_WSI_DIRECTFB_SUPPORT OFF CACHE BOOL "" FORCE)
        add_subdirectory("${AQUILA_VULKAN_LOADER_DIR}" SYSTEM)
        set_target_properties(vulkan PROPERTIES DEBUG_POSTFIX "")

        add_library(AquilaVulkan INTERFACE)
        target_link_libraries(AquilaVulkan INTERFACE vulkan Vulkan::Headers)
        add_library(Vulkan::Vulkan ALIAS AquilaVulkan)
        set(Vulkan_FOUND TRUE)
        set(Vulkan_INCLUDE_DIRS "${AQUILA_VULKAN_HEADERS_DIR}/include")
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
