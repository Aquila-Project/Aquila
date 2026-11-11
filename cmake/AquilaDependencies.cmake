set(VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE PATH "Vulkan SDK root (optional)")
if (VULKAN_SDK AND NOT DEFINED ENV{VULKAN_SDK})
    set(ENV{VULKAN_SDK} "${VULKAN_SDK}")
endif ()

include(AquilaSubmodules)

include(AquilaVulkan)
include(AquilaSystemPackages)

aquila_provide_vulkan()

include(AquilaSlang)
aquila_provide_slang()

aquila_detect_validation_layers(_aquila_layers_available)
option(AQUILA_VALIDATION_LAYERS "Enable Vulkan validation layers in Debug and RelWithDebInfo builds" ${_aquila_layers_available})
if (AQUILA_VALIDATION_LAYERS AND NOT _aquila_layers_available)
    message(WARNING "Aquila: validation layers are enabled but VkLayer_khronos_validation was not found; Debug builds will fail to start without them.")
endif ()

function(aquila_deploy_runtime target)
    if (NOT WIN32)
        return()
    endif ()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target}>
            ${AQUILA_SLANG_RUNTIME_FILES}
            $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime libraries next to ${target}"
    )
endfunction()
