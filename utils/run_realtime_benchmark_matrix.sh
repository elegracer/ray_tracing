#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 <output-root> [frames]" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_ROOT="$1"
FRAMES="${2:-3}"
BIN="${REPO_ROOT}/bin/render_realtime"

mkdir -p "${OUTPUT_ROOT}"

for PROFILE in balanced realtime; do
    for CAMERAS in 1 2 4; do
        RUN_DIR="${OUTPUT_ROOT}/${PROFILE}-c${CAMERAS}"
        mkdir -p "${RUN_DIR}"
        "${BIN}" \
            --camera-count "${CAMERAS}" \
            --frames "${FRAMES}" \
            --profile "${PROFILE}" \
            --skip-image-write \
            --output-dir "${RUN_DIR}"
    done
done
