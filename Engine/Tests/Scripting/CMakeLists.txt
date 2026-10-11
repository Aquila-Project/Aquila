set(TEST_NAME ScriptingTests)

file(GLOB_RECURSE TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_compile_features(${TEST_NAME} PRIVATE cxx_std_20)

target_include_directories(${TEST_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Include
    ${CMAKE_SOURCE_DIR}/Engine/Vendor/doctest
)

target_precompile_headers(${TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Include/BasePCH.h
)

target_link_libraries(${TEST_NAME}
    PRIVATE
    Scripting
)

aquila_deploy_runtime(${TEST_NAME})

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
set(ALL_TEST_TARGETS ${ALL_TEST_TARGETS} ${TEST_NAME} PARENT_SCOPE)
set(ALL_TEST_MODULES ${ALL_TEST_MODULES} "Scripting" PARENT_SCOPE)
