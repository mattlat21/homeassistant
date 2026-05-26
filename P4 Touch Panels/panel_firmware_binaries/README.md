# Panel firmware binaries

Staged ESP-IDF build outputs for the touch panels. Each release lives in its own folder:

```text
panel_firmware_binaries/
  v1.1.1/
    esp_hmi.bin
    bootloader.bin
    partition-table.bin
    flash_args
    flash_project_args
    manifest.json
  v1.2.0/
    ...
```

Folders are created by [`scripts/build_panel_firmware_binaries.sh`](../scripts/build_panel_firmware_binaries.sh) from `panel_firmware/build/` after `idf.py build`. The version (`vX.Y.Z`) matches `FW_VER_MAJOR` / `FW_VER_MINOR` / `FW_VER_PATCH` in [`panel_firmware/CMakeLists.txt`](../panel_firmware/CMakeLists.txt).

To publish a build over SFTP, use [`scripts/upload_firmware_sftp.sh`](../scripts/upload_firmware_sftp.sh) (uploads the app `.bin` for the chosen or latest version).

## Files in each `vX.Y.Z/` folder

| File | What it is |
|------|------------|
| **`esp_hmi.bin`** | **Application firmware** — the main program image. This is what panels use for **HTTPS OTA** (MQTT command with a URL pointing at this file). Name follows the ESP-IDF project name (`esp_hmi`). |
| **`bootloader.bin`** | **Second-stage bootloader** — runs on chip reset, loads the partition table and app. Only needed for a **full serial flash**, not for OTA of the app alone. |
| **`partition-table.bin`** | **Partition table** — defines flash layout (OTA slots `ota_0` / `ota_1`, `otadata`, etc.). Flash together with the bootloader when doing a **factory or full reflash**; changing partitions usually requires erase + full flash, not app-only OTA. |
| **`flash_args`** | **esptool argument list** for flashing **individual** images at fixed offsets (bootloader, app, partition table, `ota_data`). Paths inside the file are relative to the ESP-IDF `build/` tree; if you run `esptool.py` from this staged folder, adjust paths or copy the missing `ota_data_initial.bin` from `panel_firmware/build/`. |
| **`flash_project_args`** | Same idea as `flash_args`, generated for **`idf.py flash`** / project flash workflows. Reference only unless you adapt paths for your environment. |
| **`manifest.json`** | **Index for this release** — version string, project name, UTC timestamp, and list of artifacts copied into the folder. Useful for scripts and humans to see what was staged. |

## OTA vs full flash

- **OTA (typical):** Ship only **`esp_hmi.bin`** (e.g. on your web/SFTP host under `…/vX.Y.Z/esp_hmi.bin`). Panels download it via the MQTT OTA URL; the running bootloader swaps the inactive OTA slot.
- **Full flash (factory, partition change, bricked device):** Use bootloader + partition table + app (and `otadata` as in `flash_args`) with `esptool.py` or `idf.py flash` from a dev machine with the panel connected over USB. See the main [README](../README.md) for ESP-IDF build/flash notes.

## Regenerating a folder

From the repo root, with ESP-IDF exported:

```bash
source "$IDF_PATH/export.sh"
./scripts/build_panel_firmware_binaries.sh
```

To re-copy from an existing build without compiling:

```bash
./scripts/build_panel_firmware_binaries.sh --skip-build
```

After bumping the version in `CMakeLists.txt`, a new `vX.Y.Z/` directory is created; older version folders are left in place for history or rollback hosting.
