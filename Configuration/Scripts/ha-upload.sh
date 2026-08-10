#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="${SCRIPT_DIR}/../config/"
DEST="/Volumes/config/"

echo "Uploading Home Assistant configuration..."
echo "From: $SOURCE"
echo "To:   $DEST"
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
echo "This will update the live Home Assistant configuration."
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

echo

if [ $RESULT -eq 0 ]; then
    echo "Upload complete."
    echo
    echo "Home Assistant has NOT been restarted."
    echo "Check the configuration before restarting Home Assistant."
else
    echo "ERROR: rsync failed with code $RESULT"
    exit $RESULT
fi