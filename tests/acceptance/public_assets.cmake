cmake_minimum_required(VERSION 3.14)

include("${CMAKE_CURRENT_LIST_DIR}/public_assets.lock.cmake")

if(NOT DEFINED MODE)
    set(MODE "verify")
endif()
if(NOT MODE STREQUAL "lock" AND NOT MODE STREQUAL "fetch" AND NOT MODE STREQUAL "verify")
    message(FATAL_ERROR "MODE must be one of: lock, fetch, verify")
endif()

function(require_lock_value variable_name)
    if(NOT DEFINED ${variable_name} OR "${${variable_name}}" STREQUAL "")
        message(FATAL_ERROR "public acceptance lock is missing ${variable_name}")
    endif()
endfunction()

function(validate_hex value expected_length description)
    string(LENGTH "${value}" actual_length)
    if(NOT actual_length EQUAL expected_length OR NOT value MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR "${description} must be ${expected_length} lowercase hex characters")
    endif()
endfunction()

if(NOT RT_PUBLIC_ACCEPTANCE_ASSET_SCHEMA STREQUAL "public_acceptance_assets_v1")
    message(FATAL_ERROR "unsupported public acceptance asset schema")
endif()

foreach(required_gate IN ITEMS
        usd_stage_import
        openpbr_material_compile
        realtime_render
        offline_render_artifacts
        multi_pose_coverage
        camera_model_coverage
        simultaneous_multiview
        deterministic_reference_image)
    list(FIND RT_PUBLIC_ACCEPTANCE_REQUIRED_GATES "${required_gate}" gate_index)
    if(gate_index EQUAL -1)
        message(FATAL_ERROR "public acceptance lock dropped required gate ${required_gate}")
    endif()
endforeach()

if(NOT RT_PUBLIC_ACCEPTANCE_RENDER_OUTPUT_SCHEMA STREQUAL
        "public_acceptance_render_outputs_v1")
    message(FATAL_ERROR "unsupported public acceptance render output schema")
endif()
if(RT_PUBLIC_ACCEPTANCE_RENDER_WIDTH LESS 1 OR RT_PUBLIC_ACCEPTANCE_RENDER_HEIGHT LESS 1)
    message(FATAL_ERROR "public acceptance render dimensions must be positive")
endif()

foreach(required_format IN ITEMS linear_exr display_png)
    list(FIND RT_PUBLIC_ACCEPTANCE_RENDER_FORMATS "${required_format}" format_index)
    if(format_index EQUAL -1)
        message(FATAL_ERROR "public acceptance dropped required format ${required_format}")
    endif()
endforeach()
foreach(required_manifest_field IN ITEMS
        source_revisions
        render_settings
        sample_seed
        cameras
        outputs
        simultaneous_submission_id
        reference_metrics)
    list(FIND RT_PUBLIC_ACCEPTANCE_MANIFEST_FIELDS
        "${required_manifest_field}" manifest_field_index)
    if(manifest_field_index EQUAL -1)
        message(FATAL_ERROR
            "public acceptance manifest dropped field ${required_manifest_field}")
    endif()
endforeach()
foreach(required_model IN ITEMS pinhole32 equi62_lut1d)
    list(FIND RT_PUBLIC_ACCEPTANCE_CAMERA_MODELS "${required_model}" model_index)
    if(model_index EQUAL -1)
        message(FATAL_ERROR "public acceptance dropped camera model ${required_model}")
    endif()
endforeach()

list(LENGTH RT_PUBLIC_ACCEPTANCE_SINGLE_VIEW_CASES single_view_count)
math(EXPR total_view_output_count
    "${single_view_count} + ${RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CAMERA_COUNT}")
list(LENGTH RT_PUBLIC_ACCEPTANCE_RENDER_FORMATS render_format_count)
math(EXPR total_image_artifact_count "${total_view_output_count} * ${render_format_count}")
if(RT_PUBLIC_ACCEPTANCE_MIN_VIEW_OUTPUTS LESS 10
        OR RT_PUBLIC_ACCEPTANCE_MIN_IMAGE_ARTIFACTS LESS 20
        OR total_view_output_count LESS RT_PUBLIC_ACCEPTANCE_MIN_VIEW_OUTPUTS
        OR total_image_artifact_count LESS RT_PUBLIC_ACCEPTANCE_MIN_IMAGE_ARTIFACTS)
    message(FATAL_ERROR "public acceptance render artifact matrix is incomplete")
endif()
foreach(required_pose IN ITEMS
        front_three_quarter rear_three_quarter elevated_three_quarter)
    list(FIND RT_PUBLIC_ACCEPTANCE_POSES "${required_pose}" pose_index)
    if(pose_index EQUAL -1)
        message(FATAL_ERROR "public acceptance dropped pose ${required_pose}")
    endif()
    foreach(required_model IN LISTS RT_PUBLIC_ACCEPTANCE_CAMERA_MODELS)
        set(required_case "${required_pose}:${required_model}")
        list(FIND RT_PUBLIC_ACCEPTANCE_SINGLE_VIEW_CASES "${required_case}" case_index)
        if(case_index EQUAL -1)
            message(FATAL_ERROR "public acceptance dropped single-view case ${required_case}")
        endif()
    endforeach()
endforeach()

if(NOT RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CASE_ID STREQUAL "orbit_4_mixed_models"
        OR NOT RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CAMERA_COUNT EQUAL 4)
    message(FATAL_ERROR "public acceptance must keep the four-camera orbit case")
endif()
list(LENGTH RT_PUBLIC_ACCEPTANCE_MULTIVIEW_AZIMUTHS_DEG multiview_azimuth_count)
list(LENGTH RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CAMERA_MODELS multiview_model_count)
if(NOT multiview_azimuth_count EQUAL RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CAMERA_COUNT
        OR NOT multiview_model_count EQUAL RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CAMERA_COUNT)
    message(FATAL_ERROR "public acceptance multiview arrays must match camera count")
endif()
foreach(required_model IN LISTS RT_PUBLIC_ACCEPTANCE_CAMERA_MODELS)
    list(FIND RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CAMERA_MODELS
        "${required_model}" multiview_model_index)
    if(multiview_model_index EQUAL -1)
        message(FATAL_ERROR "public multiview case dropped camera model ${required_model}")
    endif()
endforeach()

foreach(asset_id IN LISTS RT_PUBLIC_ACCEPTANCE_ASSET_IDS)
    set(prefix "RT_PUBLIC_ACCEPTANCE_ASSET_${asset_id}")
    foreach(field IN ITEMS REPOSITORY REVISION SPARSE_PATH TREE TREE_LISTING_SHA256 CACHE_DIR
            LICENSE FILE_COUNT BYTE_COUNT COVERAGE FILES)
        require_lock_value("${prefix}_${field}")
    endforeach()

    set(repository "${${prefix}_REPOSITORY}")
    if(NOT repository MATCHES "^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
        message(FATAL_ERROR "${asset_id} repository must be a credential-free GitHub identity")
    endif()
    validate_hex("${${prefix}_REVISION}" 40 "${asset_id} revision")
    validate_hex("${${prefix}_TREE}" 40 "${asset_id} sparse tree")
    validate_hex("${${prefix}_TREE_LISTING_SHA256}" 64 "${asset_id} tree listing SHA-256")

    set(cache_dir "${${prefix}_CACHE_DIR}")
    if(cache_dir MATCHES "(^|/)\\.\\.(/|$)" OR IS_ABSOLUTE "${cache_dir}")
        message(FATAL_ERROR "${asset_id} contains unsafe cache directory ${cache_dir}")
    endif()

    set(file_pairs "${${prefix}_FILES}")
    list(LENGTH file_pairs file_pair_count)
    math(EXPR file_pair_remainder "${file_pair_count} % 2")
    if(file_pair_count LESS 2 OR NOT file_pair_remainder EQUAL 0)
        message(FATAL_ERROR "${asset_id} file lock must contain path/hash pairs")
    endif()
    math(EXPR last_file_index "${file_pair_count} - 2")
    foreach(file_index RANGE 0 ${last_file_index} 2)
        math(EXPR hash_index "${file_index} + 1")
        list(GET file_pairs ${file_index} relative_path)
        list(GET file_pairs ${hash_index} expected_sha256)
        if(relative_path MATCHES "(^|/)\\.\\.(/|$)" OR IS_ABSOLUTE "${relative_path}")
            message(FATAL_ERROR "${asset_id} contains unsafe file path ${relative_path}")
        endif()
        validate_hex("${expected_sha256}" 64 "${asset_id}:${relative_path} SHA-256")
    endforeach()
endforeach()

require_lock_value("RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_ENTRYPOINT")
set(usd_entrypoint "${RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_ENTRYPOINT}")
if(usd_entrypoint MATCHES "(^|/)\\.\\.(/|$)" OR IS_ABSOLUTE "${usd_entrypoint}")
    message(FATAL_ERROR "usd_wg_vehicles contains an unsafe entrypoint")
endif()

if(MODE STREQUAL "lock")
    message(STATUS "public acceptance asset lock is valid")
    return()
endif()

if(NOT DEFINED ROOT OR ROOT STREQUAL "")
    message(FATAL_ERROR "ROOT is required for public acceptance asset fetch or verification")
endif()
get_filename_component(ROOT "${ROOT}" ABSOLUTE)
find_program(GIT_EXECUTABLE git)
if(NOT GIT_EXECUTABLE)
    message(FATAL_ERROR "git is required to fetch and verify public acceptance assets")
endif()

if(MODE STREQUAL "fetch")
    file(MAKE_DIRECTORY "${ROOT}")
endif()

foreach(asset_id IN LISTS RT_PUBLIC_ACCEPTANCE_ASSET_IDS)
    set(prefix "RT_PUBLIC_ACCEPTANCE_ASSET_${asset_id}")
    set(repository "${${prefix}_REPOSITORY}")
    set(repository_url "https://github.com/${repository}.git")
    set(revision "${${prefix}_REVISION}")
    set(sparse_path "${${prefix}_SPARSE_PATH}")
    set(checkout "${ROOT}/${${prefix}_CACHE_DIR}")

    if(MODE STREQUAL "fetch" AND NOT EXISTS "${checkout}/.git")
        if(EXISTS "${checkout}")
            message(FATAL_ERROR "refusing to replace non-git cache path ${checkout}")
        endif()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -c core.autocrlf=false clone --filter=blob:none
                --no-checkout --sparse "${repository_url}" "${checkout}"
            RESULT_VARIABLE clone_result
        )
        if(NOT clone_result EQUAL 0)
            message(FATAL_ERROR "failed to clone ${repository}")
        endif()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${checkout}" sparse-checkout set "${sparse_path}"
            RESULT_VARIABLE sparse_result
        )
        if(NOT sparse_result EQUAL 0)
            message(FATAL_ERROR "failed to select ${repository}:${sparse_path}")
        endif()
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${checkout}" checkout --detach "${revision}"
            RESULT_VARIABLE checkout_result
        )
        if(NOT checkout_result EQUAL 0)
            message(FATAL_ERROR "failed to check out ${repository}@${revision}")
        endif()
    endif()

    if(NOT EXISTS "${checkout}/.git")
        message(FATAL_ERROR
            "missing ${asset_id}; run the fetch_public_acceptance_assets target first")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${checkout}" remote get-url origin
        OUTPUT_VARIABLE actual_repository_url
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE remote_result
    )
    if(NOT remote_result EQUAL 0 OR NOT actual_repository_url STREQUAL repository_url)
        message(FATAL_ERROR "${asset_id} cache has an unexpected origin")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${checkout}" rev-parse HEAD
        OUTPUT_VARIABLE actual_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE revision_result
    )
    if(NOT revision_result EQUAL 0 OR NOT actual_revision STREQUAL revision)
        message(FATAL_ERROR "${asset_id} cache is not pinned to ${revision}")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${checkout}" status --porcelain --untracked-files=no
        OUTPUT_VARIABLE dirty_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE status_result
    )
    if(NOT status_result EQUAL 0 OR NOT dirty_output STREQUAL "")
        message(FATAL_ERROR "${asset_id} cache contains tracked modifications")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${checkout}" rev-parse "${revision}:${sparse_path}"
        OUTPUT_VARIABLE actual_tree
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE tree_result
    )
    if(NOT tree_result EQUAL 0 OR NOT actual_tree STREQUAL "${${prefix}_TREE}")
        message(FATAL_ERROR "${asset_id} sparse tree does not match the lock")
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${checkout}" ls-tree -r -l "${revision}:${sparse_path}"
        OUTPUT_VARIABLE tree_listing
        RESULT_VARIABLE listing_result
    )
    if(NOT listing_result EQUAL 0)
        message(FATAL_ERROR "failed to inspect ${asset_id} tree")
    endif()
    string(SHA256 actual_listing_sha256 "${tree_listing}")
    if(NOT actual_listing_sha256 STREQUAL "${${prefix}_TREE_LISTING_SHA256}")
        message(FATAL_ERROR "${asset_id} tree listing SHA-256 does not match the lock")
    endif()

    string(REGEX MATCHALL "[^\n]+" tree_lines "${tree_listing}")
    list(LENGTH tree_lines actual_file_count)
    set(actual_byte_count 0)
    foreach(tree_line IN LISTS tree_lines)
        string(REGEX MATCH " ([0-9]+)\t" size_match "${tree_line}")
        if(NOT size_match)
            message(FATAL_ERROR "failed to parse ${asset_id} tree entry")
        endif()
        math(EXPR actual_byte_count "${actual_byte_count} + ${CMAKE_MATCH_1}")
    endforeach()
    if(NOT actual_file_count EQUAL "${${prefix}_FILE_COUNT}"
            OR NOT actual_byte_count EQUAL "${${prefix}_BYTE_COUNT}")
        message(FATAL_ERROR "${asset_id} file count or byte count does not match the lock")
    endif()

    set(file_pairs "${${prefix}_FILES}")
    list(LENGTH file_pairs file_pair_count)
    math(EXPR last_file_index "${file_pair_count} - 2")
    foreach(file_index RANGE 0 ${last_file_index} 2)
        math(EXPR hash_index "${file_index} + 1")
        list(GET file_pairs ${file_index} relative_path)
        list(GET file_pairs ${hash_index} expected_sha256)
        set(asset_file "${checkout}/${relative_path}")
        if(NOT EXISTS "${asset_file}")
            message(FATAL_ERROR "${asset_id} is missing ${relative_path}")
        endif()
        file(SHA256 "${asset_file}" actual_sha256)
        if(NOT actual_sha256 STREQUAL expected_sha256)
            message(FATAL_ERROR "${asset_id}:${relative_path} SHA-256 does not match the lock")
        endif()
    endforeach()

    if(asset_id STREQUAL "openpbr_examples")
        file(GLOB openpbr_materials "${checkout}/examples/*.mtlx")
        list(LENGTH openpbr_materials openpbr_material_count)
        if(NOT openpbr_material_count EQUAL 83)
            message(FATAL_ERROR "OpenPBR acceptance corpus must contain all 83 example materials")
        endif()
        foreach(material_file IN LISTS openpbr_materials)
            file(READ "${material_file}" material_source)
            string(FIND "${material_source}" "<materialx version=\"1.39\"" version_offset)
            string(FIND "${material_source}" "<open_pbr_surface " surface_offset)
            string(FIND "${material_source}"
                "nodename=\"open_pbr_surface_surfaceshader\"" binding_offset)
            if(version_offset EQUAL -1 OR surface_offset EQUAL -1 OR binding_offset EQUAL -1)
                message(FATAL_ERROR "${material_file} is not an OpenPBR 1.39 material document")
            endif()
        endforeach()
    endif()

    message(STATUS
        "verified ${asset_id}: ${actual_file_count} files, ${actual_byte_count} bytes, ${revision}")
endforeach()
