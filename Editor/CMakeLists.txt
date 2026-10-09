set(EDITOR_ROOT_DIR    ${CMAKE_CURRENT_SOURCE_DIR})
set(EDITOR_INCLUDE_DIR ${EDITOR_ROOT_DIR}/Include)
set(EDITOR_SOURCE_DIR  ${EDITOR_ROOT_DIR}/Source)

file(GLOB_RECURSE EDITOR_SOURCES
    ${EDITOR_SOURCE_DIR}/*.cpp
)

if (WIN32)
    list(APPEND EDITOR_SOURCES ${CMAKE_SOURCE_DIR}/app.rc)
endif()

add_executable(AquilaEditor ${EDITOR_SOURCES})

set_target_properties(AquilaEditor PROPERTIES
    OUTPUT_NAME "Aquila Studio"
    DEBUG_POSTFIX "(Debug)"
)

target_include_directories(AquilaEditor
    PRIVATE
    ${EDITOR_INCLUDE_DIR}
    ${EDITOR_SOURCE_DIR}
)

target_precompile_headers(AquilaEditor PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Include/GraphicsPCH.h
)

aquila_link_engine(AquilaEditor)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(AquilaEditor PRIVATE
        ${AQUILA_CLANG_WALL} -Wextra
        -Wno-unused-parameter
        -Wno-deprecated-literal-operator
        -fcolor-diagnostics
        $<$<CONFIG:Release>:-gline-tables-only>
    )
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(AquilaEditor PRIVATE
        -Wall -Wextra
        -Wno-unused-parameter
        $<$<CONFIG:Debug>:-O0 -g3 -fno-omit-frame-pointer>
        $<$<CONFIG:Release>:-O3 -g1>
    )
endif()

if (EXISTS ${CMAKE_SOURCE_DIR}/aquila-logo.ico)
    add_custom_command(TARGET AquilaEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/aquila-logo.ico
            $<TARGET_FILE_DIR:AquilaEditor>/aquila-logo.ico
        COMMENT "Copying Aquila logo"
    )
endif()

foreach (_aquila_content IN ITEMS Assets Resources)
    if (_aquila_content STREQUAL "Assets")
        set(_aquila_content_source ${AQUILA_ASSET_PATH})
    else ()
        set(_aquila_content_source ${AQUILA_RESOURCES_PATH})
    endif ()
    if (EXISTS ${_aquila_content_source})
        add_custom_command(TARGET AquilaEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${_aquila_content_source} $<TARGET_FILE_DIR:AquilaEditor>/${_aquila_content}
            COMMENT "Copying ${_aquila_content}"
            VERBATIM
        )
    endif ()
endforeach ()
