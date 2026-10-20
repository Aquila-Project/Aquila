include_guard(GLOBAL)

set(AQUILA_PLAYER_EXECUTABLE_NAME "AquilaPlayer")
set(AQUILA_PLAYER_TEMPLATE_DIR "Templates/Player")
set(AQUILA_PLAYER_MANIFEST "Data/game.aqbuild")
set(AQUILA_PLAYER_ENGINE_RESOURCES_DIR "Data/Engine/Resources")
set(AQUILA_PLAYER_ENGINE_SHADERS_DIR "Data/Engine/Shaders")
set(AQUILA_PLAYER_PROJECT_DIR "Data/Project")
set(AQUILA_PLAYER_ICON "Data/icon.png")
if (WIN32)
    set(AQUILA_PLAYER_LIBRARIES_DIR ".")
else ()
    set(AQUILA_PLAYER_LIBRARIES_DIR "Data/Engine/Lib")
endif ()

function(aquila_player_layout_definitions target)
    target_compile_definitions(${target} PUBLIC
        AQUILA_PLAYER_EXECUTABLE_NAME="${AQUILA_PLAYER_EXECUTABLE_NAME}${CMAKE_EXECUTABLE_SUFFIX}"
        AQUILA_PLAYER_EXECUTABLE_SUFFIX="${CMAKE_EXECUTABLE_SUFFIX}"
        AQUILA_PLAYER_TEMPLATE_DIR="${AQUILA_PLAYER_TEMPLATE_DIR}"
        AQUILA_PLAYER_MANIFEST="${AQUILA_PLAYER_MANIFEST}"
        AQUILA_PLAYER_ENGINE_RESOURCES_DIR="${AQUILA_PLAYER_ENGINE_RESOURCES_DIR}"
        AQUILA_PLAYER_ENGINE_SHADERS_DIR="${AQUILA_PLAYER_ENGINE_SHADERS_DIR}"
        AQUILA_PLAYER_PROJECT_DIR="${AQUILA_PLAYER_PROJECT_DIR}"
        AQUILA_PLAYER_ICON="${AQUILA_PLAYER_ICON}"
    )
endfunction()

function(aquila_player_runtime_layout target)
    if (WIN32)
        target_sources(${target} PRIVATE ${CMAKE_SOURCE_DIR}/app.rc)
    else ()
        set_target_properties(${target} PROPERTIES
            BUILD_WITH_INSTALL_RPATH ON
            INSTALL_RPATH "$ORIGIN/${AQUILA_PLAYER_LIBRARIES_DIR}"
        )
    endif ()
endfunction()

function(_aquila_player_slang_runtime out_files out_dirs)
    set(_files "")
    set(_search_dirs "")
    if (WIN32)
        set(_files ${AQUILA_SLANG_RUNTIME_FILES})
        foreach (_file IN LISTS _files)
            get_filename_component(_dir "${_file}" DIRECTORY)
            list(APPEND _search_dirs "${_dir}")
        endforeach ()
    else ()
        get_target_property(_slang Slang::Slang IMPORTED_LOCATION)
        get_filename_component(_dir "${_slang}" DIRECTORY)
        list(APPEND _search_dirs "${_dir}")
        file(GLOB _candidates "${_dir}/libslang*.so*")
        foreach (_file IN LISTS _candidates)
            if (NOT IS_SYMLINK "${_file}")
                list(APPEND _files "${_file}")
            endif ()
        endforeach ()
    endif ()

    set(_dirs "")
    list(REMOVE_DUPLICATES _search_dirs)
    foreach (_dir IN LISTS _search_dirs)
        file(GLOB _modules LIST_DIRECTORIES true "${_dir}/slang-standard-module-*")
        foreach (_module IN LISTS _modules)
            if (IS_DIRECTORY "${_module}")
                list(APPEND _dirs "${_module}")
            endif ()
        endforeach ()
    endforeach ()

    set(${out_files} "${_files}" PARENT_SCOPE)
    set(${out_dirs} "${_dirs}" PARENT_SCOPE)
endfunction()

function(aquila_stage_player_template)
    if (NOT TARGET AquilaPlayer)
        return()
    endif ()

    if (TARGET AquilaEditor)
        set(_root "$<TARGET_FILE_DIR:AquilaEditor>/${AQUILA_PLAYER_TEMPLATE_DIR}")
        set(_where "next to the editor")
    else ()
        set(_root "${CMAKE_BINARY_DIR}/${AQUILA_PLAYER_TEMPLATE_DIR}")
        set(_where "in the build folder")
    endif ()

    _aquila_player_slang_runtime(_slang_files _slang_dirs)
    set(_libraries ${_slang_files} "$<TARGET_RUNTIME_DLLS:AquilaPlayer>")
    if (AQUILA_SHARED_ENGINE AND NOT WIN32)
        list(APPEND _libraries "$<TARGET_FILE:AquilaRuntime>")
    endif ()

    set(_lib_root "${_root}/${AQUILA_PLAYER_LIBRARIES_DIR}")
    set(_commands
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_root}" "${_lib_root}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:AquilaPlayer>" "${_root}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${_libraries} "${_lib_root}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${AQUILA_RESOURCES_PATH}" "${_root}/${AQUILA_PLAYER_ENGINE_RESOURCES_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/Engine/Shaders" "${_root}/${AQUILA_PLAYER_ENGINE_SHADERS_DIR}"
    )
    foreach (_dir IN LISTS _slang_dirs)
        get_filename_component(_name "${_dir}" NAME)
        list(APPEND _commands COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different "${_dir}" "${_lib_root}/${_name}")
    endforeach ()

    add_custom_target(PlayerTemplate ALL
        ${_commands}
        DEPENDS AquilaPlayer
        COMMENT "[AQUILA] Staging the player template"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    add_dependencies(PlayerTemplate AquilaPlayer)
    if (TARGET AquilaEditor)
        add_dependencies(AquilaEditor PlayerTemplate)
    endif ()

    list(LENGTH _slang_files _slang_count)
    message(STATUS "Player template: ${AQUILA_PLAYER_TEMPLATE_DIR} ${_where} (${_slang_count} Slang libraries)")
endfunction()
