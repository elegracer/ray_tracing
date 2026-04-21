#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
exec "${BUILD_DIR}/derive_default_camera_intrinsics" "$@"
