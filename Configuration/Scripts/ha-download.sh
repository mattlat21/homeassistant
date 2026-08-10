#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="/Volumes/config/"
DEST="${SCRIPT_DIR}/../config/"

echo "Downloading Home Assistant configuration..."
echo "From: $SOURCE"
echo "To:   $DEST"
echo

if [ ! -d "$SOURCE" ]; then
    echo "ERROR: Home Assistant config share is not mounted at:"
    echo "$SOURCE"
    exit 1
fi

if [ ! -d "$DEST" ]; then
    echo "ERROR: Local configuration folder does not exist:"
    echo "$DEST"
    exit 1
fi

rsync -av \
    --delete \
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
    echo "Download complete."
    echo
    cd "$DEST" || exit 1
    git status
else
    echo "ERROR: rsync failed with code $RESULT"
    exit $RESULT
fi