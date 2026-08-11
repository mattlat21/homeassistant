#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="${SCRIPT_DIR}/../config/"
DEST="/Volumes/config/"

LOVELACE_FILES=(
    lovelace.lovelace
    lovelace_dashboards
    lovelace_resources
)

echo "Uploading Home Assistant configuration..."
echo "From: $SOURCE"
echo "To:   $DEST"
echo
echo "Includes versioned Lovelace dashboards:"
for file in "${LOVELACE_FILES[@]}"; do
    echo "  - .storage/$file"
done
echo

if [ ! -d "$SOURCE" ]; then
    echo "ERROR: Local configuration folder does not exist:"
    echo "$SOURCE"
    exit 1
fi

if [ ! -d "$DEST" ]; then
    echo "ERROR: Home Assistant config share is not mounted at:"
    echo "$DEST"
    exit 1
fi

echo "WARNING:"
echo "This will update the live Home Assistant configuration,"
echo "including Lovelace dashboards."
echo
read -p "Continue? [y/N] " CONFIRM

case "$CONFIRM" in
    y|Y|yes|YES)
        ;;
    *)
        echo "Cancelled."
        exit 0
        ;;
esac

rsync -av \
    --exclude='.git/' \
    --exclude='secrets.yaml' \
    --exclude='.storage/' \
    --exclude='.cloud/' \
    --exclude='home-assistant_v2.db*' \
    --exclude='home-assistant.log*' \
    --exclude='*.log' \
    --exclude='backups/' \
    --exclude='backup/' \
    --exclude='tts/' \
    --exclude='deps/' \
    --exclude='__pycache__/' \
    --exclude='*.pyc' \
    --exclude='.DS_Store' \
    "$SOURCE" "$DEST"

RESULT=$?

if [ $RESULT -eq 0 ]; then
    for file in "${LOVELACE_FILES[@]}"; do
        if [ ! -f "${SOURCE}.storage/$file" ]; then
            echo "ERROR: Missing local Lovelace file:"
            echo "${SOURCE}.storage/$file"
            exit 1
        fi
        rsync -av "${SOURCE}.storage/$file" "${DEST}.storage/$file"
        FILE_RESULT=$?
        if [ $FILE_RESULT -ne 0 ]; then
            RESULT=$FILE_RESULT
        fi
    done
fi

echo

if [ $RESULT -eq 0 ]; then
    echo "Upload complete."
    echo
    echo "Home Assistant has NOT been restarted."
    echo "Check the configuration before restarting Home Assistant."
    echo "Refresh the dashboard if Lovelace changes do not appear."
else
    echo "ERROR: rsync failed with code $RESULT"
    exit $RESULT
fi
