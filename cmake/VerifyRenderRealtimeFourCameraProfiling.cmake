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
if(NOT run_stdout MATCHES "cameras=4")
    message(FATAL_ERROR "stdout missing 4-camera frame line:\n${run_stdout}")
endif()
if(NOT run_stdout MATCHES "host_overhead_ms=")
    message(FATAL_ERROR "stdout missing host_overhead_ms field:\n${run_stdout}")
endif()
if(NOT run_stdout MATCHES "image_write_ms=0\\.000")
    message(FATAL_ERROR "stdout missing skip-write zero image_write_ms field:\n${run_stdout}")
endif()

file(GLOB smoke_pngs "${OUTPUT_DIR}/*.png")
list(LENGTH smoke_pngs smoke_png_count)
if(NOT smoke_png_count EQUAL 0)
    message(FATAL_ERROR "skip-write 4-camera run still wrote PNG files")
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
if(NOT json_text MATCHES "\"camera_count\": 4")
    message(FATAL_ERROR "json missing 4-camera metadata:\n${json_text}")
endif()
if(NOT json_text MATCHES "\"camera_index\": 3")
    message(FATAL_ERROR "json missing camera 3 record:\n${json_text}")
endif()
if(NOT json_text MATCHES "\"per-camera\"")
    message(FATAL_ERROR "json missing per-camera records:\n${json_text}")
endif()

set(token_f0_c0 "\"frame_index\": 0, \"camera_index\": 0")
set(token_f0_c1 "\"frame_index\": 0, \"camera_index\": 1")
set(token_f0_c2 "\"frame_index\": 0, \"camera_index\": 2")
set(token_f0_c3 "\"frame_index\": 0, \"camera_index\": 3")
set(token_f1_c0 "\"frame_index\": 1, \"camera_index\": 0")
set(token_f1_c1 "\"frame_index\": 1, \"camera_index\": 1")
set(token_f1_c2 "\"frame_index\": 1, \"camera_index\": 2")
set(token_f1_c3 "\"frame_index\": 1, \"camera_index\": 3")

string(FIND "${json_text}" "${token_f0_c0}" pos_f0_c0)
string(FIND "${json_text}" "${token_f0_c1}" pos_f0_c1)
string(FIND "${json_text}" "${token_f0_c2}" pos_f0_c2)
string(FIND "${json_text}" "${token_f0_c3}" pos_f0_c3)
string(FIND "${json_text}" "${token_f1_c0}" pos_f1_c0)
string(FIND "${json_text}" "${token_f1_c1}" pos_f1_c1)
string(FIND "${json_text}" "${token_f1_c2}" pos_f1_c2)
string(FIND "${json_text}" "${token_f1_c3}" pos_f1_c3)

if(pos_f0_c0 LESS 0 OR pos_f0_c1 LESS 0 OR pos_f0_c2 LESS 0 OR pos_f0_c3 LESS 0
    OR pos_f1_c0 LESS 0 OR pos_f1_c1 LESS 0 OR pos_f1_c2 LESS 0 OR pos_f1_c3 LESS 0)
    message(FATAL_ERROR "json missing expected ordered per-camera frame records:\n${json_text}")
endif()
if(NOT (pos_f0_c0 LESS pos_f0_c1
    AND pos_f0_c1 LESS pos_f0_c2
    AND pos_f0_c2 LESS pos_f0_c3
    AND pos_f0_c3 LESS pos_f1_c0
    AND pos_f1_c0 LESS pos_f1_c1
    AND pos_f1_c1 LESS pos_f1_c2
    AND pos_f1_c2 LESS pos_f1_c3))
    message(FATAL_ERROR "per-camera records are not deterministically ordered by frame_index,camera_index")
endif()

string(REGEX MATCH "\"host_overhead_ms\": \\{\"avg\": ([0-9]+\\.?[0-9]*)" host_overhead_match "${json_text}")
if(NOT host_overhead_match)
    message(FATAL_ERROR "json missing host_overhead_ms avg:\n${json_text}")
endif()
set(host_overhead_avg "${CMAKE_MATCH_1}")
if(host_overhead_avg GREATER 80.0)
    message(FATAL_ERROR
        "host_overhead_ms.avg=${host_overhead_avg} exceeds pooled-path budget (80.0).")
endif()
