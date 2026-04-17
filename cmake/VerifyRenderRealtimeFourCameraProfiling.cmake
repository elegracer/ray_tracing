if(NOT DEFINED RENDER_REALTIME_EXE)
    message(FATAL_ERROR "RENDER_REALTIME_EXE is required")
endif()
if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()
if(NOT DEFINED PROFILE_NAME)
    set(PROFILE_NAME realtime)
endif()
if(NOT DEFINED SCENE_NAME)
    set(SCENE_NAME smoke)
endif()
if(NOT DEFINED FRAME_COUNT)
    set(FRAME_COUNT 2)
endif()
if(NOT DEFINED EXPECT_DENOISE_ENABLED)
    set(EXPECT_DENOISE_ENABLED ON)
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(
    COMMAND "${RENDER_REALTIME_EXE}"
        --scene "${SCENE_NAME}"
        --camera-count 4
        --frames "${FRAME_COUNT}"
        --profile "${PROFILE_NAME}"
        --skip-image-write
        --output-dir "${OUTPUT_DIR}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "render_realtime 4-camera run failed: ${run_stderr}")
endif()

if(NOT run_stdout MATCHES "denoise_ms=")
    message(FATAL_ERROR "stdout missing denoise_ms field:\n${run_stdout}")
endif()
if(NOT EXPECT_DENOISE_ENABLED)
    string(REGEX MATCHALL "frame=[0-9]+ cameras=4 [^\r\n]*denoise_ms=0\\.000" zero_denoise_lines "${run_stdout}")
    list(LENGTH zero_denoise_lines zero_denoise_count)
    if(NOT zero_denoise_count EQUAL FRAME_COUNT)
        message(FATAL_ERROR
            "quality 4-camera run expected ${FRAME_COUNT} zero-denoise frame lines:\n${run_stdout}")
    endif()
endif()

set(csv_path "${OUTPUT_DIR}/benchmark_frames.csv")
set(json_path "${OUTPUT_DIR}/benchmark_summary.json")
if(NOT EXISTS "${csv_path}")
    message(FATAL_ERROR "missing ${csv_path}")
endif()
if(NOT EXISTS "${json_path}")
    message(FATAL_ERROR "missing ${json_path}")
endif()

file(READ "${json_path}" json_text)
set(json_flat "${json_text}")
string(REGEX REPLACE "[\r\n\t ]+" " " json_flat "${json_flat}")

string(REGEX MATCH "\"metadata\" *: *\\{[^\\{\\}]*\\}" metadata_object_match "${json_flat}")
if(NOT metadata_object_match)
    message(FATAL_ERROR "json missing metadata object")
endif()
set(metadata_object "${CMAKE_MATCH_0}")

string(REGEX MATCH "\"camera_count\" *: *([0-9]+)" metadata_camera_count_match "${metadata_object}")
if(NOT metadata_camera_count_match)
    message(FATAL_ERROR "metadata missing camera_count")
endif()
set(metadata_camera_count "${CMAKE_MATCH_1}")
if(NOT metadata_camera_count EQUAL 4)
    message(FATAL_ERROR "metadata.camera_count expected 4, got ${metadata_camera_count}")
endif()
string(REGEX MATCH "\"profile\" *: *\"([^\"]+)\"" metadata_profile_match "${metadata_object}")
if(NOT metadata_profile_match)
    message(FATAL_ERROR "metadata missing profile")
endif()
set(metadata_profile "${CMAKE_MATCH_1}")
if(NOT metadata_profile STREQUAL PROFILE_NAME)
    message(FATAL_ERROR "metadata.profile expected ${PROFILE_NAME}, got ${metadata_profile}")
endif()
string(REGEX MATCH "\"denoise_enabled\" *: *(true|false)" metadata_denoise_match "${metadata_object}")
if(NOT metadata_denoise_match)
    message(FATAL_ERROR "metadata missing denoise_enabled")
endif()
set(metadata_denoise_enabled "${CMAKE_MATCH_1}")
if(EXPECT_DENOISE_ENABLED)
    if(NOT metadata_denoise_enabled STREQUAL "true")
        message(FATAL_ERROR "metadata.denoise_enabled expected true, got ${metadata_denoise_enabled}")
    endif()
else()
    if(NOT metadata_denoise_enabled STREQUAL "false")
        message(FATAL_ERROR "metadata.denoise_enabled expected false, got ${metadata_denoise_enabled}")
    endif()
endif()

string(REGEX MATCHALL "\\{[^\\{\\}]*\"frame_index\" *: *[0-9]+[^\\{\\}]*\"camera_index\" *: *[0-9]+[^\\{\\}]*\\}"
    per_camera_records "${json_flat}")
list(LENGTH per_camera_records per_camera_length)
math(EXPR expected_record_count "${FRAME_COUNT} * 4")
if(NOT per_camera_length EQUAL expected_record_count)
    message(FATAL_ERROR "per-camera record count expected ${expected_record_count}, got ${per_camera_length}")
endif()

set(record_index 0)
math(EXPR last_frame_index "${FRAME_COUNT} - 1")
foreach(expected_frame_index RANGE 0 ${last_frame_index})
    foreach(expected_camera_index RANGE 0 3)
        list(GET per_camera_records ${record_index} record_text)
        string(REGEX MATCH "\"frame_index\" *: *([0-9]+)" actual_frame_match "${record_text}")
        string(REGEX MATCH "\"camera_index\" *: *([0-9]+)" actual_camera_match "${record_text}")
        if(NOT actual_frame_match OR NOT actual_camera_match)
            message(FATAL_ERROR "per-camera record ${record_index} missing frame_index/camera_index fields")
        endif()

        string(REGEX REPLACE ".*\"frame_index\" *: *([0-9]+).*" "\\1" actual_frame_index "${record_text}")
        string(REGEX REPLACE ".*\"camera_index\" *: *([0-9]+).*" "\\1" actual_camera_index "${record_text}")

        if(NOT actual_frame_index EQUAL expected_frame_index OR NOT actual_camera_index EQUAL expected_camera_index)
            message(FATAL_ERROR
                "deterministic per-camera ordering mismatch at record ${record_index}: "
                "expected (frame_index=${expected_frame_index}, camera_index=${expected_camera_index}), "
                "got (frame_index=${actual_frame_index}, camera_index=${actual_camera_index})")
        endif()
        math(EXPR record_index "${record_index} + 1")
    endforeach()
endforeach()
