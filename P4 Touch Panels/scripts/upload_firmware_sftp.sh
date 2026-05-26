#!/usr/bin/env bash
#
# Upload a panel firmware .bin to SFTP (Nearly Free Speech or any SFTP host).
#
# Reads SFTP_HOST, SFTP_USER, SFTP_PASSWORD, SFTP_REMOTE_DIR from
# scripts/upload_firmware_sftp.env (or UPLOAD_FIRMWARE_ENV_FILE / --env-file).
#
# Local binaries: panel_firmware_binaries/vX.Y.Z/ (from build_panel_firmware_binaries.sh).
# If --version is omitted, the latest semver folder is used.
#
# Remote layout:
#   <SFTP_REMOTE_DIR>/vX.Y.Z/<project>.bin
#
# Requires: sshpass (e.g. brew install hudochenkov/sshpass/sshpass)
#
# Usage (from repo root):
#   ./scripts/upload_firmware_sftp.sh
#   ./scripts/upload_firmware_sftp.sh --version 1.1.1
#   ./scripts/upload_firmware_sftp.sh --dry-run
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BINARIES_ROOT="${WORKSPACE_ROOT}/panel_firmware_binaries"

VERSION_ARG=""
ENV_FILE_TO_SOURCE=""
DRY_RUN=0

usage() {
  cat <<'EOF'
Usage: upload_firmware_sftp.sh [--version X.Y.Z] [--env-file PATH] [--dry-run]

  --version X.Y.Z   Use panel_firmware_binaries/vX.Y.Z/ (default: latest)
  --env-file PATH   SFTP credentials file (default: scripts/upload_firmware_sftp.env)
  --dry-run         Print resolved paths without uploading
EOF
}

# Resolve --env-file early so we can source before other defaults.
_pre_args=("$@")
for ((idx = 0; idx < ${#_pre_args[@]}; idx++)); do
  t="${_pre_args[idx]}"
  if [[ "$t" == "--env-file" ]]; then
    next=$((idx + 1))
    if [[ $next -lt ${#_pre_args[@]} ]]; then
      ENV_FILE_TO_SOURCE="${_pre_args[next]}"
    fi
  elif [[ "$t" == --env-file=* ]]; then
    ENV_FILE_TO_SOURCE="${t#*=}"
  fi
done

while [[ $# -gt 0 ]]; do
  case "$1" in
    --version)
      if [[ $# -lt 2 ]]; then echo "ERROR: --version requires a value." >&2; exit 2; fi
      VERSION_ARG="$2"
      shift 2
      ;;
    --version=*)
      VERSION_ARG="${1#*=}"
      shift
      ;;
    --env-file)
      shift 2
      ;;
    --env-file=*)
      shift
      ;;
    --dry-run)
      DRY_RUN=1
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

if [[ -z "${ENV_FILE_TO_SOURCE}" ]]; then
  ENV_FILE_TO_SOURCE="${UPLOAD_FIRMWARE_ENV_FILE:-${SCRIPT_DIR}/upload_firmware_sftp.env}"
fi

if [[ ! -f "${ENV_FILE_TO_SOURCE}" ]]; then
  echo "error: missing env file: ${ENV_FILE_TO_SOURCE}" >&2
  echo "hint: cp scripts/upload_firmware_sftp.env.example scripts/upload_firmware_sftp.env" >&2
  exit 1
fi

echo "Loading environment from ${ENV_FILE_TO_SOURCE}"
set -a
# shellcheck disable=SC1090
source "${ENV_FILE_TO_SOURCE}"
set +a

SFTP_HOST="${SFTP_HOST:-}"
SFTP_USER="${SFTP_USER:-}"
SFTP_PASSWORD="${SFTP_PASSWORD:-}"
SFTP_REMOTE_DIR="${SFTP_REMOTE_DIR:-root/ha/fware/esphmi}"
SFTP_PORT="${SFTP_PORT:-22}"

if [[ -z "${SFTP_HOST}" || -z "${SFTP_USER}" || -z "${SFTP_PASSWORD}" ]]; then
  echo "error: SFTP_HOST, SFTP_USER, and SFTP_PASSWORD must be set in ${ENV_FILE_TO_SOURCE}" >&2
  exit 1
fi

normalize_version() {
  local v="$1"
  v="${v#v}"
  v="${v#V}"
  if [[ ! "${v}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "error: invalid version '${1}'; expected X.Y.Z or vX.Y.Z" >&2
    exit 1
  fi
  printf '%s' "${v}"
}

version_sort_key() {
  # Print zero-padded sortable key: major minor patch
  local v="$1"
  local a b c
  IFS='.' read -r a b c <<<"${v}"
  printf '%08d.%08d.%08d' "${a}" "${b}" "${c}"
}

resolve_latest_version() {
  local best="" best_key="" d name ver key
  if [[ ! -d "${BINARIES_ROOT}" ]]; then
    echo "error: no firmware under ${BINARIES_ROOT}; run build_panel_firmware_binaries.sh first" >&2
    exit 1
  fi
  for d in "${BINARIES_ROOT}"/v*/; do
    [[ -d "${d}" ]] || continue
    name="$(basename "${d}")"
    if [[ ! "${name}" =~ ^[vV][0-9]+\.[0-9]+\.[0-9]+$ ]]; then
      continue
    fi
    ver="$(normalize_version "${name}")"
    key="$(version_sort_key "${ver}")"
    if [[ -z "${best}" || "${key}" > "${best_key}" ]]; then
      best="${ver}"
      best_key="${key}"
    fi
  done
  if [[ -z "${best}" ]]; then
    echo "error: no firmware under ${BINARIES_ROOT}; run build_panel_firmware_binaries.sh first" >&2
    exit 1
  fi
  printf '%s' "${best}"
}

if [[ -n "${VERSION_ARG}" ]]; then
  VER_STR="$(normalize_version "${VERSION_ARG}")"
else
  VER_STR="$(resolve_latest_version)"
fi

VERSION_DIR="${BINARIES_ROOT}/v${VER_STR}"
if [[ ! -d "${VERSION_DIR}" ]]; then
  echo "error: missing ${VERSION_DIR}" >&2
  exit 1
fi

PROJECT="esp_hmi"
MANIFEST="${VERSION_DIR}/manifest.json"
if [[ -f "${MANIFEST}" ]]; then
  _proj="$(grep -E '"project"[[:space:]]*:' "${MANIFEST}" | head -1 | sed -E 's/.*"project"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/' || true)"
  if [[ -n "${_proj}" ]]; then
    PROJECT="${_proj}"
  fi
fi

LOCAL_BIN="${VERSION_DIR}/${PROJECT}.bin"
if [[ ! -f "${LOCAL_BIN}" ]]; then
  LOCAL_BIN=""
  largest_size=0
  for candidate in "${VERSION_DIR}"/*.bin; do
    [[ -f "${candidate}" ]] || continue
    base="$(basename "${candidate}")"
    case "${base}" in
      bootloader.bin|partition-table.bin|ota_data_initial.bin) continue ;;
    esac
    size="$(stat -f%z "${candidate}" 2>/dev/null || stat -c%s "${candidate}")"
    if [[ "${size}" -gt "${largest_size}" ]]; then
      largest_size="${size}"
      LOCAL_BIN="${candidate}"
    fi
  done
fi

if [[ -z "${LOCAL_BIN}" || ! -f "${LOCAL_BIN}" ]]; then
  echo "error: no app .bin in ${VERSION_DIR}" >&2
  exit 1
fi

REMOTE_BASE="${SFTP_REMOTE_DIR#/}"
REMOTE_BASE="${REMOTE_BASE%/}"
REMOTE_DIR="${REMOTE_BASE}/v${VER_STR}"
REMOTE_PATH="${REMOTE_DIR}/$(basename "${LOCAL_BIN}")"

if [[ "${DRY_RUN}" -eq 1 ]]; then
  echo "dry-run: would upload ${LOCAL_BIN}"
  echo "         -> sftp://${SFTP_USER}@${SFTP_HOST}:${SFTP_PORT}/${REMOTE_PATH}"
  echo "would upload firmware v${VER_STR}"
  echo "  local:  ${LOCAL_BIN}"
  echo "  remote: sftp://${SFTP_USER}@${SFTP_HOST}/${REMOTE_PATH}"
  exit 0
fi

if ! command -v sshpass >/dev/null 2>&1; then
  echo "error: sshpass is required for password-based SFTP upload." >&2
  echo "  brew install hudochenkov/sshpass/sshpass" >&2
  exit 1
fi

sftp_mkdir_p() {
  local remote_path="$1"
  local current="" part
  local batch
  batch="$(mktemp)"
  trap 'rm -f "${batch}"' RETURN

  IFS='/' read -r -a _parts <<<"${remote_path}"
  for part in "${_parts[@]}"; do
    [[ -n "${part}" ]] || continue
    current="${current:+$current/}${part}"
    printf 'mkdir %s\n' "${current}" >>"${batch}"
  done

  SSHPASS="${SFTP_PASSWORD}" sshpass -e sftp \
    -oBatchMode=no \
    -oStrictHostKeyChecking=accept-new \
    -P "${SFTP_PORT}" \
    -b "${batch}" \
    "${SFTP_USER}@${SFTP_HOST}" \
    2>/dev/null || true
}

sftp_upload() {
  local batch
  batch="$(mktemp)"
  trap 'rm -f "${batch}"' RETURN
  printf 'put "%s" "%s"\n' "${LOCAL_BIN}" "${REMOTE_PATH}" >"${batch}"

  SSHPASS="${SFTP_PASSWORD}" sshpass -e sftp \
    -oBatchMode=no \
    -oStrictHostKeyChecking=accept-new \
    -P "${SFTP_PORT}" \
    -b "${batch}" \
    "${SFTP_USER}@${SFTP_HOST}"
}

sftp_mkdir_p "${REMOTE_DIR}"
sftp_upload

echo "uploaded firmware v${VER_STR}"
echo "  local:  ${LOCAL_BIN}"
echo "  remote: sftp://${SFTP_USER}@${SFTP_HOST}/${REMOTE_PATH}"
echo "  HTTPS (if mapped): https://<your-site>/${REMOTE_PATH}"
