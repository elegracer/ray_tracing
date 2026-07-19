if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()
if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

set(source_revision "unknown")
set(source_dirty 0)
set(source_state_sha256 "unknown")
find_program(GIT_EXECUTABLE git)
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --verify HEAD
        OUTPUT_VARIABLE revision_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE revision_result
    )
    if(revision_result EQUAL 0 AND NOT revision_output STREQUAL "")
        set(source_revision "${revision_output}")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" status --porcelain --untracked-files=all
            -- CMakeLists.txt cmake src utils
        OUTPUT_VARIABLE status_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE status_result
    )
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" diff --no-ext-diff --binary HEAD
            -- CMakeLists.txt cmake src utils
        OUTPUT_VARIABLE diff_output
        ERROR_QUIET
        RESULT_VARIABLE diff_result
    )
    if(status_result EQUAL 0 AND diff_result EQUAL 0)
        if(NOT status_output STREQUAL "")
            set(source_dirty 1)
        endif()
        string(SHA256 source_state_sha256 "${status_output}\n${diff_output}")
    endif()
endif()

get_filename_component(output_dir "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")
set(output_tmp "${OUTPUT_FILE}.tmp")
file(WRITE "${output_tmp}"
    "#pragma once\n\n"
    "#define RT_SOURCE_REVISION \"${source_revision}\"\n"
    "#define RT_SOURCE_DIRTY ${source_dirty}\n"
    "#define RT_SOURCE_SCOPE \"CMakeLists.txt,cmake,src,utils\"\n"
    "#define RT_SOURCE_STATE_SHA256 \"${source_state_sha256}\"\n"
)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${output_tmp}" "${OUTPUT_FILE}"
    RESULT_VARIABLE copy_result
)
file(REMOVE "${output_tmp}")
if(NOT copy_result EQUAL 0)
    message(FATAL_ERROR "failed to update ${OUTPUT_FILE}")
endif()
