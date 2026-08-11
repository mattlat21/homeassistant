#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE="${SCRIPT_DIR}/../config/.storage"
DEST="/Volumes/config/.storage"

LOVELACE_FILES=(
    lovelace.lovelace
    lovelace_dashboards
    lovelace_resources
)

echo "Uploading Home Assistant Lovelace dashboards..."
echo "From: $SOURCE"
echo "To:   $DEST"
echo
echo "Files:"
for file in "${LOVELACE_FILES[@]}"; do
    echo "  - $file"
done
echo

if [ ! -d "$SOURCE" ]; then
    echo "ERROR: Local .storage folder does not exist:"
    echo "$SOURCE"
    exit 1
fi

if [ ! -d "$DEST" ]; then
    echo "ERROR: Home Assistant .storage share is not mounted at:"
    echo "$DEST"
    exit 1
fi

for file in "${LOVELACE_FILES[@]}"; do
    if [ ! -f "$SOURCE/$file" ]; then
        echo "ERROR: Missing local Lovelace file:"
        echo "$SOURCE/$file"
        exit 1
    fi
done

echo "WARNING:"
echo "This will update the live Lovelace dashboards only."
echo "Other .storage files will not be uploaded."
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

RESULT=0
for file in "${LOVELACE_FILES[@]}"; do
    rsync -av "$SOURCE/$file" "$DEST/$file"
    FILE_RESULT=$?
    if [ $FILE_RESULT -ne 0 ]; then
        RESULT=$FILE_RESULT
    fi
done

echo

if [ $RESULT -eq 0 ]; then
    echo "Lovelace upload complete."
    echo
    echo "Home Assistant has NOT been restarted."
    echo "Refresh the dashboard, or restart Home Assistant if changes do not appear."
else
    echo "ERROR: rsync failed with code $RESULT"
    exit $RESULT
fi
