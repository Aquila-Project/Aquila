add_subdirectory(Platform)
add_subdirectory(Foundation)
add_subdirectory(RHI)
add_subdirectory(UI)
add_subdirectory(Scene)
add_subdirectory(Scripting)

set(TEST_COMMANDS "")
list(LENGTH ALL_TEST_TARGETS TARGET_COUNT)
math(EXPR LAST_INDEX "${TARGET_COUNT} - 1")

foreach(i RANGE ${LAST_INDEX})
    list(GET ALL_TEST_TARGETS ${i} TARGET)
    list(GET ALL_TEST_MODULES ${i} MODULE)
    list(APPEND TEST_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E echo "[AQUILA] Testing module: [${MODULE}]"
        COMMAND $<TARGET_FILE:${TARGET}> --no-intro --force-colors=true
    )
endforeach()

list(APPEND TEST_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E echo "==============================================================================="
)

add_custom_target(RunAllTests ALL
    ${TEST_COMMANDS}
    DEPENDS ${ALL_TEST_TARGETS}
    COMMENT "Running all tests..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
