#!/usr/bin/env bash
#
# Deploy the ESP HMI Home Assistant custom component via Samba (SMB/CIFS).
#
# Intended for Home Assistant OS / Yellow with the "Samba share" add-on: the `config`
# share maps to /config on the appliance. Files are copied to:
#   <share root>/custom_components/esp_hmi/
# which is the same as /config/custom_components/esp_hmi/ on HA.
#
# Clean updates (same as before):
# - Everything under custom_components/esp_hmi/ on the share is removed, then replaced
#   with a fresh copy from this repo so deleted files disappear on HA.
#
# --------------------------------------------------------------------------------------
# PREREQUISITES
#
# 1) Samba add-on enabled; share name is usually `config` (maps to /config).
# 2) LAN access to SMB (TCP 445) from this machine to HA_HOST.
# 3) Samba username + password from the add-on configuration (not the same as SSH).
#
# macOS: uses mount_smbfs (built-in).
# Linux: uses mount -t cifs (needs sudo and usually: apt install cifs-utils).
#
# --------------------------------------------------------------------------------------
# SAFETY
#
# - Only the directory custom_components/esp_hmi under the mounted share is wiped.
# - Before rm, we resolve realpath and require the last path component to be `esp_hmi`.
# - Mount is always unmounted via trap on exit/error.
#
# --------------------------------------------------------------------------------------
# OPTIONAL: --restart (SSH only)
#
# Samba cannot restart Home Assistant. If you pass --restart, this script tries SSH:
#   ha core restart
# on HA_SSH_USER@HA_HOST (same defaults as before). Requires SSH access + sshpass if
# you use SSH password. This does NOT use remote rsync.
#
# --------------------------------------------------------------------------------------
# USAGE
#
#   SMB_USER=... SMB_PASSWORD=... ./scripts/deploy_esp_hmi_to_ha.sh
#   ./scripts/deploy_esp_hmi_to_ha.sh --smb-user ha --smb-password 'secret'
#
# ENV FILE (recommended):
#   cp scripts/deploy_esp_hmi_to_ha.env.example scripts/deploy_esp_hmi_to_ha.env
#   # edit SMB_USER / SMB_PASSWORD (use quotes if password contains & etc.)
#   ./scripts/deploy_esp_hmi_to_ha.sh --dry-run
#   Override file path: DEPLOY_ENV_FILE=/path/to/.env ./scripts/deploy_esp_hmi_to_ha.sh
#   Or: ./scripts/deploy_esp_hmi_to_ha.sh --env-file /path/to/.env --dry-run
#
#   ./scripts/deploy_esp_hmi_to_ha.sh --dry-run
#
#   ./scripts/deploy_esp_hmi_to_ha.sh --restart
#   (needs SSH key or HA_SSH_PASSWORD + sshpass)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load optional env file before applying defaults / parsing args.
# Resolution order: first --env-file in argv, else DEPLOY_ENV_FILE env, else
# scripts/deploy_esp_hmi_to_ha.env next to this script.
ENV_FILE_TO_SOURCE=""
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
if [[ -z "${ENV_FILE_TO_SOURCE}" ]]; then
  ENV_FILE_TO_SOURCE="${DEPLOY_ENV_FILE:-${SCRIPT_DIR}/deploy_esp_hmi_to_ha.env}"
fi
if [[ -f "${ENV_FILE_TO_SOURCE}" ]]; then
  echo "Loading environment from ${ENV_FILE_TO_SOURCE}"
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE_TO_SOURCE}"
  set +a
fi

# -------------------------
# Paths (local repo)
# -------------------------
LOCAL_SRC_DIR="custom_components/esp_hmi/"

# Relative path on the config share (share root == /config).
SHARE_REL_DEST="custom_components/esp_hmi"

# -------------------------
# SMB defaults (env overridable)
# -------------------------
SMB_HOST="${SMB_HOST:-${HA_HOST:-192.168.1.52}}"
SMB_SHARE="${SMB_SHARE:-config}"
SMB_USER="${SMB_USER:-}"
SMB_PASSWORD="${SMB_PASSWORD:-}"

# -------------------------
# Optional SSH restart
# -------------------------
HA_HOST="${HA_HOST:-192.168.1.52}"
HA_SSH_USER="${HA_SSH_USER:-root}"
HA_SSH_PORT="${HA_SSH_PORT:-22}"
HA_SSH_PASSWORD="${HA_SSH_PASSWORD:-}"

# -------------------------
# Arg parsing
# -------------------------
DRY_RUN=0
RESTART_AFTER=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --env-file)
      if [[ $# -lt 2 ]]; then echo "ERROR: --env-file requires a path."; exit 2; fi
      shift 2
      ;;
    --env-file=*)
      shift
      ;;
    --dry-run)
      DRY_RUN=1
      shift
      ;;
    --restart)
      RESTART_AFTER=1
      shift
      ;;
    --smb-user)
      if [[ $# -lt 2 ]]; then echo "ERROR: --smb-user requires a value."; exit 2; fi
      SMB_USER="$2"
      shift 2
      ;;
    --smb-user=*)
      SMB_USER="${1#*=}"
      shift
      ;;
    --smb-password)
      if [[ $# -lt 2 ]]; then echo "ERROR: --smb-password requires a value."; exit 2; fi
      SMB_PASSWORD="$2"
      shift 2
      ;;
    --smb-password=*)
      SMB_PASSWORD="${1#*=}"
      shift
      ;;
    --share)
      if [[ $# -lt 2 ]]; then echo "ERROR: --share requires a value."; exit 2; fi
      SMB_SHARE="$2"
      shift 2
      ;;
    --share=*)
      SMB_SHARE="${1#*=}"
      shift
      ;;
    --host)
      if [[ $# -lt 2 ]]; then echo "ERROR: --host requires a value."; exit 2; fi
      SMB_HOST="$2"
      HA_HOST="$2"
      shift 2
      ;;
    --host=*)
      SMB_HOST="${1#*=}"
      HA_HOST="${1#*=}"
      shift
      ;;
    --ssh-password)
      if [[ $# -lt 2 ]]; then echo "ERROR: --ssh-password requires a value."; exit 2; fi
      HA_SSH_PASSWORD="$2"
      shift 2
      ;;
    --ssh-password=*)
      HA_SSH_PASSWORD="${1#*=}"
      shift
      ;;
    -h|--help)
      cat <<'EOF'
Usage: deploy_esp_hmi_to_ha.sh [OPTIONS]

Deploy ./custom_components/esp_hmi/ to the HA config share via SMB (Samba add-on).

Options:
  --env-file PATH         Load SMB_* (and optional SSH_*) from a file (also auto-loads
                          scripts/deploy_esp_hmi_to_ha.env if it exists).
  --dry-run               Print actions only; no mount, no delete, no copy.
  --smb-user USER         Samba username (or SMB_USER).
  --smb-password PASS     Samba password (or SMB_PASSWORD).
  --share NAME            SMB share name (default: config). Or SMB_SHARE.
  --host ADDR             HA IP/hostname (default: 192.168.1.52). Or SMB_HOST / HA_HOST.
  --restart               After SMB deploy, try SSH: ha core restart (optional).
  --ssh-password PASS     SSH password for --restart (or HA_SSH_PASSWORD; needs sshpass).

Environment:
  SMB_HOST, SMB_SHARE, SMB_USER, SMB_PASSWORD
  HA_HOST, HA_SSH_USER, HA_SSH_PORT, HA_SSH_PASSWORD
  DEPLOY_ENV_FILE         Default env file path if not using --env-file

Examples:
  cp scripts/deploy_esp_hmi_to_ha.env.example scripts/deploy_esp_hmi_to_ha.env
  ./scripts/deploy_esp_hmi_to_ha.sh --dry-run

  SMB_USER=ha SMB_PASSWORD='secret' ./scripts/deploy_esp_hmi_to_ha.sh --dry-run
  ./scripts/deploy_esp_hmi_to_ha.sh --smb-user ha --smb-password 'secret'
EOF
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Try: $0 --help"
      exit 2
      ;;
  esac
done

# -------------------------
# SMB URL encode (user/password for mount_smbfs // URL)
# -------------------------
smb_url_escape() {
  if command -v python3 >/dev/null 2>&1; then
    python3 -c 'import urllib.parse,sys; print(urllib.parse.quote(sys.argv[1], safe=""))' "$1"
  else
    # Fallback: pass through (may break if password contains @ : / # etc.)
    printf '%s' "$1"
  fi
}

# -------------------------
# Local sanity
# -------------------------
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_ROOT"

if [[ ! -d "$LOCAL_SRC_DIR" ]]; then
  echo "ERROR: Local source folder not found: $REPO_ROOT/$LOCAL_SRC_DIR"
  echo "Run this script from the repo root or via scripts/ as usual."
  exit 1
fi

if [[ -z "${SMB_USER}" || -z "${SMB_PASSWORD}" ]]; then
  echo "ERROR: Samba credentials required."
  echo "Set SMB_USER and SMB_PASSWORD (or use --smb-user / --smb-password)."
  exit 1
fi

echo "SMB host:      $SMB_HOST"
echo "SMB share:     $SMB_SHARE"
echo "SMB user:      $SMB_USER"
echo "Local source:  $REPO_ROOT/$LOCAL_SRC_DIR"
echo "Share path:    //$SMB_HOST/$SMB_SHARE/$SHARE_REL_DEST/"
echo

# -------------------------
# Dry-run: no network writes
# -------------------------
if [[ "$DRY_RUN" -eq 1 ]]; then
  echo "DRY-RUN: No mount, delete, or copy will be performed."
  echo "Would clear then refill: //$SMB_HOST/$SMB_SHARE/$SHARE_REL_DEST/"
  echo
  echo "Local files that would be copied:"
  (cd "$LOCAL_SRC_DIR" && find . -type f | LC_ALL=C sort | sed 's|^./||')
  echo
  echo "End dry-run."
  exit 0
fi

# -------------------------
# mount_smbfs (macOS) or cifs (Linux)
# -------------------------
MNT=""
cleanup_mount() {
  if [[ -n "${MNT}" && -d "${MNT}" ]]; then
    if [[ "$(uname -s)" == "Darwin" ]]; then
      umount "${MNT}" 2>/dev/null || diskutil unmount force "${MNT}" 2>/dev/null || true
    else
      if mountpoint -q "${MNT}" 2>/dev/null; then
        sudo umount "${MNT}" 2>/dev/null || true
      fi
    fi
    rmdir "${MNT}" 2>/dev/null || true
  fi
}

MNT="$(mktemp -d "${TMPDIR:-/tmp}/esp_hmi_smb.XXXXXX")"
trap cleanup_mount EXIT

OS_KIND="$(uname -s)"
PING_RC=-1
NC445_RC=-1
HAVE_NC=0
if [[ "$DRY_RUN" -eq 0 ]]; then
  set +e
  # H2: host ICMP reachability (best-effort; may be blocked yet TCP still works)
  if ping -c 1 -W 3000 "${SMB_HOST}" >/dev/null 2>&1; then
    PING_RC=0
  else
    PING_RC=$?
  fi
  # H1/H3/H5: SMB port reachable on LAN
  if command -v nc >/dev/null 2>&1; then
    HAVE_NC=1
    if nc -z -G 8 "${SMB_HOST}" 445 >/dev/null 2>&1; then
      NC445_RC=0
    else
      NC445_RC=$?
    fi
  fi
  set -e

  # Runtime evidence showed ping OK but nc:445 failing while mount_smbfs timed out —
  # fail fast with actionable guidance instead of a long ambiguous mount error.
  if [[ "$HAVE_NC" -eq 1 && "$NC445_RC" -ne 0 ]]; then
    echo "ERROR: SMB is not reachable on ${SMB_HOST}:445 (TCP check failed)." >&2
    echo "" >&2
    echo "Your host replies to ping, but nothing accepted a connection on port 445 from this Mac." >&2
    echo "Fix on Home Assistant Yellow / addon side, then retry:" >&2
    echo "  - Ensure the \"Samba share\" add-on is running." >&2
    echo "  - Allow SMB on the LAN (no firewall blocking TCP 445 to the HA host)." >&2
    echo "  - HA OS: Settings → Network / host firewall rules if enabled." >&2
    echo "  - Optionally verify from this Mac:  nc -zv ${SMB_HOST} 445" >&2
    exit 2
  fi
fi

if [[ "$OS_KIND" == "Darwin" ]]; then
  ENC_USER="$(smb_url_escape "$SMB_USER")"
  ENC_PASS="$(smb_url_escape "$SMB_PASSWORD")"
  SMB_URL="//${ENC_USER}:${ENC_PASS}@${SMB_HOST}/${SMB_SHARE}"
  echo "Mounting SMB share (macOS mount_smbfs)..."
  set +e
  mount_smbfs -o nobrowse "$SMB_URL" "$MNT"
  MOUNT_RC=$?
  set -e
  if [[ "$MOUNT_RC" -ne 0 ]]; then
    exit "${MOUNT_RC}"
  fi
else
  if ! command -v mount.cifs >/dev/null 2>&1; then
    echo "ERROR: Linux requires mount.cifs (package: cifs-utils)."
    exit 1
  fi
  CRED_FILE="$(mktemp "${TMPDIR:-/tmp}/smbcred.XXXXXX")"
  chmod 600 "$CRED_FILE"
  printf 'username=%s\npassword=%s\n' "$SMB_USER" "$SMB_PASSWORD" >"$CRED_FILE"
  trap 'rm -f "$CRED_FILE"; cleanup_mount' EXIT
  echo "Mounting SMB share (Linux CIFS)..."
  sudo mount -t cifs "//${SMB_HOST}/${SMB_SHARE}" "$MNT" -o "credentials=${CRED_FILE},uid=$(id -u),gid=$(id -g),vers=3.0,file_mode=0644,dir_mode=0755"
  rm -f "$CRED_FILE"
  trap cleanup_mount EXIT
fi

DEST="${MNT}/${SHARE_REL_DEST}"

# Ensure parents exist on share
mkdir -p "${MNT}/custom_components"

# Safety: destination must resolve to .../esp_hmi
mkdir -p "$DEST"
RESOLVED="$(cd "$DEST" && pwd -P)"
if [[ "$(basename "$RESOLVED")" != "esp_hmi" ]]; then
  echo "ERROR: Refusing to continue; resolved path does not end with esp_hmi: $RESOLVED"
  exit 1
fi

echo "Cleaning $RESOLVED ..."
# Remove contents only (not the esp_hmi directory itself)
find "$RESOLVED" -mindepth 1 -delete

echo "Copying from $LOCAL_SRC_DIR ..."
# Trailing slash: copy directory contents into DEST
if [[ "$OS_KIND" == "Darwin" ]] && command -v ditto >/dev/null 2>&1; then
  ditto "$LOCAL_SRC_DIR" "$DEST/"
else
  cp -R "${LOCAL_SRC_DIR}/." "$DEST/"
fi

echo
echo "Deploy complete (via Samba). Restart Home Assistant from UI if needed."

# -------------------------
# Optional SSH restart (not via Samba)
# -------------------------
if [[ "$RESTART_AFTER" -eq 1 ]]; then
  echo
  echo "OPTIONAL RESTART: trying SSH (${HA_SSH_USER}@${HA_HOST}) ..."

  SSH_OPTS=(-p "${HA_SSH_PORT}" -o ConnectTimeout=10)
  if [[ -z "${HA_SSH_PASSWORD}" ]]; then
    SSH_OPTS+=(-o BatchMode=yes)
  fi

  run_ssh() {
    if [[ -n "${HA_SSH_PASSWORD}" ]]; then
      if ! command -v sshpass >/dev/null 2>&1; then
        echo "ERROR: --restart with SSH password requires sshpass."
        exit 1
      fi
      SSHPASS="${HA_SSH_PASSWORD}" sshpass -e ssh "${SSH_OPTS[@]}" "$@"
    else
      ssh "${SSH_OPTS[@]}" "$@"
    fi
  }

  set +e
  run_ssh "${HA_SSH_USER}@${HA_HOST}" "ha core restart"
  rc=$?
  set -e
  if [[ "$rc" -ne 0 ]]; then
    echo "NOTE: Could not run 'ha core restart' over SSH. Restart manually from HA UI."
  else
    echo "Restart command sent."
  fi
fi
