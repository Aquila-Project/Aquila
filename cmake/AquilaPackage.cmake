foreach (_var BUILD_DIR FOLDER ARCHIVE)
    if (NOT DEFINED ${_var})
        message(FATAL_ERROR "AquilaPackage.cmake needs -D${_var}=...")
    endif ()
endforeach ()

set(_stage "${BUILD_DIR}/Package")
file(REMOVE_RECURSE "${_stage}/${FOLDER}")
file(REMOVE "${BUILD_DIR}/${ARCHIVE}")
file(MAKE_DIRECTORY "${_stage}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${_stage}/${FOLDER}" --component "${FOLDER}"
    RESULT_VARIABLE _result
)
if (NOT _result EQUAL 0)
    message(FATAL_ERROR "Installing into ${_stage}/${FOLDER} failed")
endif ()

if (ARCHIVE MATCHES "\\.zip$")
    set(_format --format=zip)
else ()
    set(_format --format=gnutar)
    set(_compress z)
endif ()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "c${_compress}f" "${BUILD_DIR}/${ARCHIVE}" ${_format} "${FOLDER}"
    WORKING_DIRECTORY "${_stage}"
    RESULT_VARIABLE _result
)
if (NOT _result EQUAL 0)
    message(FATAL_ERROR "Archiving ${FOLDER} into ${ARCHIVE} failed")
endif ()

message(STATUS "Packaged ${BUILD_DIR}/${ARCHIVE}")
