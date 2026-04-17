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
        --camera-count 1
        --frames 2
        --profile realtime
        --output-dir "${OUTPUT_DIR}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "render_realtime failed: ${run_stderr}")
endif()
if(NOT run_stdout MATCHES "download_ms=")
    message(FATAL_ERROR "stdout missing download_ms field:\n${run_stdout}")
endif()
if(NOT run_stdout MATCHES "host_overhead_ms=")
    message(FATAL_ERROR "stdout missing host_overhead_ms field:\n${run_stdout}")
endif()

set(csv_path "${OUTPUT_DIR}/benchmark_frames.csv")
set(json_path "${OUTPUT_DIR}/benchmark_summary.json")
if(NOT EXISTS "${csv_path}")
    message(FATAL_ERROR "missing ${csv_path}")
endif()
if(NOT EXISTS "${json_path}")
    message(FATAL_ERROR "missing ${json_path}")
endif()

file(READ "${csv_path}" csv_text)
if(NOT csv_text MATCHES "frame_index,camera_count,profile,width,height,samples_per_pixel,max_bounces,denoise_enabled")
    message(FATAL_ERROR "csv header is incomplete:\n${csv_text}")
endif()

file(READ "${json_path}" json_text)
if(NOT json_text MATCHES "\"download_ms\"")
    message(FATAL_ERROR "json missing download timing:\n${json_text}")
endif()
if(NOT json_text MATCHES "\"per-camera\"")
    message(FATAL_ERROR "json missing per-camera records:\n${json_text}")
endif()

if(DEFINED EXPECT_SKIP_WRITE AND EXPECT_SKIP_WRITE)
    file(REMOVE_RECURSE "${OUTPUT_DIR}")
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")

    execute_process(
        COMMAND "${RENDER_REALTIME_EXE}"
            --camera-count 1
            --frames 2
            --profile realtime
            --skip-image-write
            --output-dir "${OUTPUT_DIR}"
        RESULT_VARIABLE skip_result
        OUTPUT_VARIABLE skip_stdout
        ERROR_VARIABLE skip_stderr
    )

    if(NOT skip_result EQUAL 0)
        message(FATAL_ERROR "skip-write render_realtime failed: ${skip_stderr}")
    endif()
    if(NOT skip_stdout MATCHES "image_write_ms=0\\.000")
        message(FATAL_ERROR "skip-write stdout missing zero image_write_ms:\n${skip_stdout}")
    endif()
    file(GLOB pngs "${OUTPUT_DIR}/*.png")
    list(LENGTH pngs png_count)
    if(NOT png_count EQUAL 0)
        message(FATAL_ERROR "skip-write run still wrote PNG files")
    endif()
endif()
