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
aquila_shipping_editor_layout(AquilaEditor)

set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
set(NFD_WAYLAND OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/Engine/Vendor/nativefiledialog-extended ${CMAKE_BINARY_DIR}/Vendor/nfd SYSTEM)
target_link_libraries(AquilaEditor PRIVATE nfd::nfd)

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

if (NOT AQUILA_SHIPPING)
    if (EXISTS ${CMAKE_SOURCE_DIR}/aquila-logo.ico)
        add_custom_command(TARGET AquilaEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${CMAKE_SOURCE_DIR}/aquila-logo.ico
                $<TARGET_FILE_DIR:AquilaEditor>/aquila-logo.ico
            COMMENT "Copying Aquila logo"
        )
    endif()

    if (EXISTS ${AQUILA_RESOURCES_PATH})
        add_custom_command(TARGET AquilaEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${AQUILA_RESOURCES_PATH} $<TARGET_FILE_DIR:AquilaEditor>/Resources
            COMMENT "Copying Resources"
            VERBATIM
        )
    endif ()
endif ()
