include_guard(GLOBAL)

option(AQUILA_SHIPPING "Build the editor as a self-contained folder that reads nothing from the source tree" OFF)
set(AQUILA_SHIPPING_FOLDER "AquilaStudio")

if (AQUILA_SHIPPING)
    if (NOT AQUILA_BUILD_EDITOR)
        message(FATAL_ERROR "AQUILA_SHIPPING packages the editor (the engine ships inside it as a library), turn AQUILA_BUILD_EDITOR on")
    endif ()
    add_compile_definitions(AQUILA_SHIPPING)
    set(AQUILA_BUILD_TESTS_DEFAULT OFF)
else ()
    set(AQUILA_BUILD_TESTS_DEFAULT ON)
endif ()

function(aquila_shipping_editor_layout target)
    if (NOT AQUILA_SHIPPING)
        return()
    endif ()
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${AQUILA_SHIPPING_FOLDER}"
    )
    if (NOT WIN32)
        set_target_properties(${target} PROPERTIES
            BUILD_WITH_INSTALL_RPATH ON
            INSTALL_RPATH "$ORIGIN/${AQUILA_PLAYER_LIBRARIES_DIR}"
        )
    endif ()
endfunction()

function(aquila_stage_shipping_editor)
    if (NOT AQUILA_SHIPPING OR NOT TARGET AquilaEditor)
        return()
    endif ()

    if (AQUILA_SHARED_ENGINE AND NOT WIN32)
        set_target_properties(AquilaRuntime PROPERTIES
            BUILD_WITH_INSTALL_RPATH ON
            INSTALL_RPATH "$ORIGIN"
        )
    endif ()

    set(_root "${CMAKE_BINARY_DIR}/${AQUILA_SHIPPING_FOLDER}")
    aquila_engine_data_commands(_commands "${_root}" AquilaEditor)
    add_custom_target(EditorData ALL
        ${_commands}
        COMMENT "[AQUILA] Staging the editor's engine data"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    if (AQUILA_SHARED_ENGINE)
        add_dependencies(EditorData AquilaRuntime)
    endif ()
    add_dependencies(AquilaEditor EditorData)

    install(TARGETS AquilaEditor RUNTIME DESTINATION . COMPONENT ${AQUILA_SHIPPING_FOLDER})
    install(DIRECTORY "${_root}/Data" "${_root}/Templates"
        DESTINATION . USE_SOURCE_PERMISSIONS COMPONENT ${AQUILA_SHIPPING_FOLDER}
    )
    if (WIN32)
        _aquila_player_slang_runtime(_slang_files _slang_dirs)
        install(FILES "$<TARGET_RUNTIME_DLLS:AquilaEditor>" ${_slang_files}
            DESTINATION . COMPONENT ${AQUILA_SHIPPING_FOLDER}
        )
        if (_slang_dirs)
            install(DIRECTORY ${_slang_dirs} DESTINATION . COMPONENT ${AQUILA_SHIPPING_FOLDER})
        endif ()
    endif ()

    string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" _platform)
    if (WIN32)
        set(_archive "${AQUILA_SHIPPING_FOLDER}-${AQUILA_VERSION_COMPOSED}-${_platform}.zip")
    else ()
        set(_archive "${AQUILA_SHIPPING_FOLDER}-${AQUILA_VERSION_COMPOSED}-${_platform}.tar.gz")
    endif ()
    add_custom_target(Package
        COMMAND "${CMAKE_COMMAND}"
                -DBUILD_DIR=${CMAKE_BINARY_DIR}
                -DFOLDER=${AQUILA_SHIPPING_FOLDER}
                -DARCHIVE=${_archive}
                -P "${CMAKE_SOURCE_DIR}/cmake/AquilaPackage.cmake"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "[AQUILA] Packaging the editor into ${_archive}"
        USES_TERMINAL
        VERBATIM
    )
    add_dependencies(Package AquilaEditor)

    message(STATUS "Shipping editor: ${AQUILA_SHIPPING_FOLDER}/ in the build folder, target Package makes ${_archive}")
endfunction()
