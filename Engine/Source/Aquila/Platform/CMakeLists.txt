set(MODULE_NAME Platform)
set(MODULE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/Engine/Include)
set(MODULE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE MODULE_SOURCES ${MODULE_SOURCE_DIR}/*.cpp)

add_library(${MODULE_NAME} STATIC ${MODULE_SOURCES})

set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "AquilaPlatform"
    DEBUG_POSTFIX "d"
)

target_compile_features(${MODULE_NAME} PUBLIC cxx_std_20)

target_include_directories(${MODULE_NAME}
    PUBLIC
    ${MODULE_INCLUDE_DIR}
    PRIVATE
    ${MODULE_SOURCE_DIR}
)

target_precompile_headers(${MODULE_NAME} PRIVATE
    ${MODULE_INCLUDE_DIR}/BasePCH.h
)

target_link_libraries(${MODULE_NAME}
    PUBLIC
    Foundation
)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${MODULE_NAME} PRIVATE
        ${AQUILA_CLANG_WALL} -Wextra -Wpedantic
        -Wno-unused-parameter
        -fcolor-diagnostics
        -Werror=missing-designated-field-initializers
    )
endif()