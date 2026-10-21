find_package(Git QUIET)

if (NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
    return()
endif ()

if (NOT GIT_FOUND)
    message(FATAL_ERROR "Git is required to fetch Aquila's submodules. Install git, or clone with: git clone --recursive <url>")
endif ()

option(AQUILA_UPDATE_SUBMODULES "Check out the submodule commits this checkout expects while configuring" ON)

function(_aquila_git dir out_var result_var)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} ${ARGN}
        WORKING_DIRECTORY "${dir}"
        OUTPUT_VARIABLE _output
        ERROR_QUIET
        RESULT_VARIABLE _result
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE "\r" "" _output "${_output}")
    set(${out_var} "${_output}" PARENT_SCOPE)
    set(${result_var} ${_result} PARENT_SCOPE)
endfunction()

function(_aquila_submodule_has_local_work dir expected out_var)
    set(${out_var} ON PARENT_SCOPE)
    _aquila_git("${dir}" _changes _result status --porcelain --untracked-files=no)
    if (NOT _result EQUAL 0 OR _changes)
        return()
    endif ()
    set(_known "")
    _aquila_git("${dir}" _ignored _result cat-file -e "${expected}^{commit}")
    if (_result EQUAL 0)
        set(_known "${expected}")
    endif ()
    _aquila_git("${dir}" _unpushed _result rev-list -n 1 HEAD --not --remotes ${_known})
    if (_result EQUAL 0 AND NOT _unpushed)
        set(${out_var} OFF PARENT_SCOPE)
    endif ()
endfunction()

function(_aquila_update_submodules repo)
    file(RELATIVE_PATH _repo_name "${CMAKE_SOURCE_DIR}" "${repo}")
    if (_repo_name)
        set(_prefix "${_repo_name}/")
    else ()
        set(_prefix "")
    endif ()

    _aquila_git("${repo}" _status _result submodule status)
    if (NOT _result EQUAL 0)
        message(FATAL_ERROR "Could not read the git submodule status of ${repo}")
    endif ()

    string(REPLACE "\n" ";" _lines "${_status}")
    set(_paths "")
    set(_update "")
    set(_missing "")
    set(_kept "")
    foreach (_line IN LISTS _lines)
        if (NOT _line MATCHES "^(.)([0-9a-f]+) ([^ ]+)")
            continue()
        endif ()
        set(_state "${CMAKE_MATCH_1}")
        set(_path "${CMAKE_MATCH_3}")
        list(APPEND _paths "${_path}")
        if (_state STREQUAL "-")
            list(APPEND _missing "${_prefix}${_path}")
            list(APPEND _update "${_path}")
        elseif (_state STREQUAL "+")
            _aquila_git("${repo}" _expected _result rev-parse ":${_path}")
            _aquila_submodule_has_local_work("${repo}/${_path}" "${_expected}" _local_work)
            if (_local_work OR NOT AQUILA_UPDATE_SUBMODULES)
                list(APPEND _kept "${_prefix}${_path}")
            else ()
                list(APPEND _update "${_path}")
            endif ()
        elseif (_state STREQUAL "U")
            list(APPEND _kept "${_prefix}${_path}")
        endif ()
    endforeach ()

    if (_missing AND NOT AQUILA_UPDATE_SUBMODULES)
        list(JOIN _missing ", " _missing_text)
        message(FATAL_ERROR "Aquila: missing submodules (${_missing_text}). Run: git submodule update --init --recursive")
    endif ()

    if (_update)
        list(TRANSFORM _update PREPEND "${_prefix}" OUTPUT_VARIABLE _update_names)
        list(JOIN _update_names ", " _update_text)
        message(STATUS "Aquila: checking out submodules (${_update_text})")
        _aquila_git("${repo}" _ignored _result submodule sync --quiet -- ${_update})
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init -- ${_update}
            WORKING_DIRECTORY "${repo}"
            RESULT_VARIABLE _result
        )
        if (NOT _result EQUAL 0)
            message(FATAL_ERROR "Aquila: git submodule update failed for ${_update_text}. "
                                "Check your network connection, then run: git submodule update --init --recursive")
        endif ()
    endif ()

    if (_kept)
        list(JOIN _kept ", " _kept_text)
        message(WARNING "Aquila: these submodules are not at the commit this checkout expects: ${_kept_text}. "
                        "They were left alone because they have local changes, unpushed commits or a merge conflict, "
                        "or AQUILA_UPDATE_SUBMODULES is OFF. "
                        "Run 'git submodule update --init --recursive' unless you changed them on purpose.")
    endif ()

    foreach (_path IN LISTS _paths)
        if (EXISTS "${repo}/${_path}/.gitmodules" AND EXISTS "${repo}/${_path}/.git")
            _aquila_update_submodules("${repo}/${_path}")
        endif ()
    endforeach ()
endfunction()

_aquila_update_submodules("${CMAKE_SOURCE_DIR}")
