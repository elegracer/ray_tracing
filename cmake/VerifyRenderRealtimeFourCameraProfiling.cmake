if(NOT DEFINED RENDER_REALTIME_EXE)
    message(FATAL_ERROR "RENDER_REALTIME_EXE is required")
endif()
if(NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "OUTPUT_DIR is required")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

execute_process(
    COMMAND "${RENDER_REALTIME_EXE}"
        --camera-count 4
        --frames 2
        --profile realtime
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

string(REGEX MATCHALL "\\{[^\\{\\}]*\"frame_index\" *: *[0-9]+[^\\{\\}]*\"camera_index\" *: *[0-9]+[^\\{\\}]*\\}"
    per_camera_records "${json_flat}")
list(LENGTH per_camera_records per_camera_length)
if(NOT per_camera_length EQUAL 8)
    message(FATAL_ERROR "per-camera record count expected 8, got ${per_camera_length}")
endif()

set(record_index 0)
foreach(expected_frame_index RANGE 0 1)
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
