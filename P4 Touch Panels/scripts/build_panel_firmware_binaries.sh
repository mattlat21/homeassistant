#!/usr/bin/env bash
#
# Build panel firmware and stage binaries under panel_firmware_binaries/vX.Y.Z/.
#
# Version is read from panel_firmware/CMakeLists.txt (FW_VER_MAJOR/MINOR/PATCH).
#
# Requires ESP-IDF on PATH:
#   source "$IDF_PATH/export.sh"
#
# Usage (from repo root):
#   ./scripts/build_panel_firmware_binaries.sh
#   ./scripts/build_panel_firmware_binaries.sh --skip-build
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FIRMWARE_ROOT="${WORKSPACE_ROOT}/panel_firmware"
CMAKE_PATH="${FIRMWARE_ROOT}/CMakeLists.txt"
BUILD_DIR="${FIRMWARE_ROOT}/build"
BINARIES_ROOT="${WORKSPACE_ROOT}/panel_firmware_binaries"

SKIP_BUILD=0

usage() {
  cat <<'EOF'
Usage: build_panel_firmware_binaries.sh [--skip-build]

  --skip-build   Copy from existing panel_firmware/build/ without running idf.py build
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERROR: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ ! -f "${CMAKE_PATH}" ]]; then
  echo "error: missing ${CMAKE_PATH}" >&2
  exit 1
fi

_cmake_val() {
  local key="$1"
  local line
  line="$(grep -E "^[[:space:]]*set[[:space:]]*\\([[:space:]]*${key}[[:space:]]+[0-9]+[[:space:]]*\\)" "${CMAKE_PATH}" | head -1 || true)"
  if [[ -z "${line}" ]]; then
    echo ""
    return 1
  fi
  sed -E "s/^[[:space:]]*set[[:space:]]*\\([[:space:]]*${key}[[:space:]]+([0-9]+)[[:space:]]*\\).*/\\1/" <<<"${line}"
}

FW_VER_MAJOR="$(_cmake_val FW_VER_MAJOR || true)"
FW_VER_MINOR="$(_cmake_val FW_VER_MINOR || true)"
FW_VER_PATCH="$(_cmake_val FW_VER_PATCH || true)"

if [[ -z "${FW_VER_MAJOR}" || -z "${FW_VER_MINOR}" || -z "${FW_VER_PATCH}" ]]; then
  echo "error: could not parse FW_VER_MAJOR/MINOR/PATCH from CMakeLists.txt" >&2
  exit 1
fi

_project_line="$(grep -E '^[[:space:]]*project[[:space:]]*\(' "${CMAKE_PATH}" | head -1 || true)"
if [[ -n "${_project_line}" ]]; then
  PROJECT_NAME="$(sed -E 's/^[[:space:]]*project[[:space:]]*\([[:space:]]*([A-Za-z0-9_]+)[[:space:]]*\).*/\1/' <<<"${_project_line}")"
else
  PROJECT_NAME="esp_hmi"
fi

VER_STR="${FW_VER_MAJOR}.${FW_VER_MINOR}.${FW_VER_PATCH}"
OUT_DIR="${BINARIES_ROOT}/v${VER_STR}"

if [[ "${SKIP_BUILD}" -eq 0 ]]; then
  if [[ -z "${IDF_PATH:-}" ]]; then
    echo "error: IDF_PATH is not set. Run: source \"\$IDF_PATH/export.sh\"" >&2
    exit 1
  fi
  echo "+ idf.py build (cwd=${FIRMWARE_ROOT})"
  (cd "${FIRMWARE_ROOT}" && idf.py build)
elif [[ ! -d "${BUILD_DIR}" ]]; then
  echo "error: ${BUILD_DIR} missing; run without --skip-build first" >&2
  exit 1
fi

APP_BIN="${BUILD_DIR}/${PROJECT_NAME}.bin"
if [[ ! -f "${APP_BIN}" ]]; then
  echo "error: missing app binary ${APP_BIN}" >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"

STAGED=()

copy_if_exists() {
  local src="$1"
  local dst_name="${2:-$(basename "${src}")}"
  if [[ -f "${src}" ]]; then
    cp -p "${src}" "${OUT_DIR}/${dst_name}"
    STAGED+=("${dst_name}")
  fi
}

copy_if_exists "${APP_BIN}"
copy_if_exists "${BUILD_DIR}/bootloader/bootloader.bin"
copy_if_exists "${BUILD_DIR}/partition_table/partition-table.bin"
copy_if_exists "${BUILD_DIR}/flash_args"
copy_if_exists "${BUILD_DIR}/flash_project_args"

GENERATED_AT_UTC="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

{
  printf '{\n'
  printf '  "version": "%s",\n' "${VER_STR}"
  printf '  "project": "%s",\n' "${PROJECT_NAME}"
  printf '  "generated_at_utc": "%s",\n' "${GENERATED_AT_UTC}"
  printf '  "artifacts": [\n'
  for ((i = 0; i < ${#STAGED[@]}; i++)); do
    if [[ $i -gt 0 ]]; then
      printf ',\n'
    fi
    printf '    "%s"' "${STAGED[$i]}"
  done
  printf '\n  ],\n'
  printf '  "note": "OTA typically uses only the app .bin over HTTPS; bootloader/partition-table are for full flash reference."\n'
  printf '}\n'
} >"${OUT_DIR}/manifest.json"

echo "Staged firmware v${VER_STR} -> ${OUT_DIR}"
while IFS= read -r -d '' entry; do
  printf '  %s\n' "$(basename "${entry}")"
done < <(find "${OUT_DIR}" -mindepth 1 -maxdepth 1 -print0 | sort -z)
