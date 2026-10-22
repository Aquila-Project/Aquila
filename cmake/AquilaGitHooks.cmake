include_guard(GLOBAL)

option(AQUILA_GIT_HOOKS "Point this clone's git at .githooks (format and lint before a commit, submodule check before a push)" ON)

function(aquila_setup_git_hooks)
    if (NOT GIT_FOUND OR NOT EXISTS "${CMAKE_SOURCE_DIR}/.git" OR NOT IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.githooks")
        return()
    endif ()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" config --local --get core.hooksPath
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _current
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if (AQUILA_GIT_HOOKS)
        if (_current STREQUAL ".githooks")
            message(STATUS "Git hooks: .githooks")
        elseif (_current STREQUAL "")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" config --local core.hooksPath .githooks
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            )
            message(STATUS "Git hooks: now using .githooks (AQUILA_GIT_HOOKS=OFF turns them off)")
        else ()
            message(STATUS "Git hooks: core.hooksPath is '${_current}', leaving it alone")
        endif ()
    elseif (_current STREQUAL ".githooks")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" config --local --unset core.hooksPath
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
        message(STATUS "Git hooks: off")
    endif ()
endfunction()

aquila_setup_git_hooks()
