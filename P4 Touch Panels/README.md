# screen_test_1

ESP-IDF + LVGL multi-screen demo for Waveshare **ESP32-P4** round touch boards: **ESP32-P4-WIFI6-Touch-LCD-3.4C** and **ESP32-P4-WIFI6-Touch-LCD-4B** (720×720 circular display via the same [Waveshare BSP](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_4b) + `esp_lvgl_port`).

## Waveshare round boards (3.4C and 4B)

Both boards use the managed component **`waveshare/esp32_p4_wifi6_touch_lcd_4b`** (Waveshare’s registry entry also documents the 3.4 / 4 inch round line). After `idf.py build`, it appears under `panel_firmware/managed_components/`.

**Panel type (menuconfig):** **Board Support Package (ESP32-P4) → Display → Select LCD type**

- **Waveshare board with 720×720 4-inch Display** — use for **720×720** round panels (typical **4B** and **3.4C** SKUs shipped as 720×720). This repo’s [`panel_firmware/sdkconfig.defaults`](panel_firmware/sdkconfig.defaults) enables this option by default.
- **Waveshare board with 800×800 3.4-inch Display** — only if your unit has the **800×800** panel. The current BSP’s `display.h` still exposes **720×720** macros in many revisions; verify on real hardware before relying on the 800×800 Kconfig path alone.

**Wi‑Fi (ESP-Hosted / C6 SDIO):** The default [`panel_firmware/sdkconfig.defaults`](panel_firmware/sdkconfig.defaults) uses **`CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD`**. If Wi‑Fi fails on a **3.4C** or **4B** layout whose SDIO pins differ from that preset, change **Component config → ESP-Hosted** to match the [3.4C wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-3.4C) or [4B wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B) schematic.

## Behavior

1. **Boot**: A **loading** screen shows an embedded **Lottie** animation (ThorVG) and a short label. After a configurable delay it switches to the **default startup screen** stored in NVS (see [Default startup screen](#default-startup-screen-nvs--mqtt); falls back to **Home** if unset/invalid) and does not return to loading until the next reset.
2. **Home**: **Launcher** grid of app tiles (see [`screen_home.c`](panel_firmware/main/ui/screens/screen_home.c)); tap a tile to open that app.
3. **Settings**: Read-only summary of **firmware version** (from build-time `FW_VER_*` ints), **Wi-Fi SSID** when associated, and the **default startup screen** (friendly name + slug).
4. **Ollie’s Room**: Grid with a **room-mode** selector (**Normal**, **Rest Time**, **Sleep Time**) synced to Home Assistant via MQTT (see below), **`ui_climate_control_1`** synced via **four shared retained MQTT topics** under `esp_hmi/data/bedroom3/climate/` (setpoint, current, heater, control) plus climate button payloads (see [Ollie climate](#home-assistant-ollie-climate--mqtt)), a **light** button, and a **fan** button that opens a modal to choose **Fan off / Low / Medium / High** (JSON `button` values `fan_off`, `fan_1` … `fan_3`). **All** panel outputs (buttons + room) publish on **`esp_hmi/device/<MAC>/status/button_press`** as **`{"button": "…"}`** (e.g. `light`, `Normal`, `climate_temp_dn`). Use MQTT discovery and the automations in [Ollie MQTT button_press](#home-assistant-ollie-mqtt-button_press) below.
5. **Gestures** (on app screens, not on loading): **swipe left/right** cycles apps in order Home → Ollie’s Room → Dashboard → Front Gate → PipBoy → Settings → Study → Home. **Swipe up** returns to **Home**.

`app_main` does not call `ui_shell_init()` directly: it uses **`lv_async_call()`** so the first Lottie/ThorVG work runs on **`taskLVGL`** (32 KiB stack) instead of the **main** task. Other code paths should still use `bsp_display_lock()` / `bsp_display_unlock()` when touching LVGL from non-LVGL tasks.

## Home Assistant: `input_select` ↔ room mode MQTT

Example automations for Home Assistant are kept under [`home_assistant/`](home_assistant/) (YAML you can paste or merge into `automations.yaml`).

Create an `input_select` (e.g. `input_select.ollie_room_desired_state`) whose options are **exactly** `Normal`, `Rest Time`, and `Sleep Time`.

**`input_select` → retained MQTT** (panels subscribe; default topic `esp_hmi/data/bedroom3/desired_state`). Copy-ready YAML: [`home_assistant/ha_automation_data_bedroom_3_mode.yaml`](home_assistant/ha_automation_data_bedroom_3_mode.yaml).

HA remains the **source of truth**; every panel **subscribes** to the same retained room state topic (unless you override it in Kconfig) and publishes room taps as **`{"button": "Normal"}`** (etc.) on **`esp_hmi/device/<MAC>/status/button_press`**. The firmware ignores MQTT state payloads that match the last applied option to avoid echo flicker.

## Home Assistant: Ollie climate ↔ MQTT

The panel **subscribes** to four **shared** topics (defaults below; full paths from Kconfig `SCREEN_TEST_MQTT_CLIMATE_SETPOINT_TOPIC`, `…_CURRENT_TOPIC`, `…_HEATER_TOPIC`, `…_CONTROL_TOPIC`). Each message is **retained**, plain text, QoS 1:

| Topic (default) | Payload | HA source (example) |
| ----------------- | ------- | ------------------- |
| `esp_hmi/data/bedroom3/climate/setpoint` | Float °C | `input_number.desired_temperature_in_ollie_s_bedroom` |
| `esp_hmi/data/bedroom3/climate/current` | Float °C | `sensor.temp0001_temperature` |
| `esp_hmi/data/bedroom3/climate/heater_on` | `true` / `false` (or `on` / `off`, `1` / `0`) | `switch.bedroom_3_heater` (HA automation example) |
| `esp_hmi/data/bedroom3/climate/control` | HA climate **state** (`heat`, `off`, …) or legacy bool | `states(climate.…)` (see [`home_assistant/ha_automation_data_bedroom_3_climate.yaml`](home_assistant/ha_automation_data_bedroom_3_climate.yaml)) |

The UI updates after **all four** topics have delivered at least one parseable value (e.g. after reconnect once retained messages are replayed).

**Automation — entities → retained climate topics** (one automation, four `mqtt.publish` steps; add/remove triggers as needed). Copy-ready YAML: [`home_assistant/ha_automation_data_bedroom_3_climate.yaml`](home_assistant/ha_automation_data_bedroom_3_climate.yaml).

Climate **− / +** and **mode** actions are handled in the same MQTT automation as light and fan: [Ollie MQTT button_press](#home-assistant-ollie-mqtt-button_press) below.

After MQTT connect, the device publishes **device_automation** discovery for all button payloads (reload MQTT or restart HA to pick up new triggers).

**MQTT contract (AsyncAPI):** [`panel_firmware/docs/asyncapi.yaml`](panel_firmware/docs/asyncapi.yaml) — topic names, QoS, retain, and payloads as consumed by the firmware. Open in [AsyncAPI Studio](https://studio.asyncapi.com/) (paste or import the file), or validate with [`@asyncapi/cli`](https://www.npmjs.com/package/@asyncapi/cli): `npx @asyncapi/cli validate panel_firmware/docs/asyncapi.yaml`.

## Home Assistant: Ollie MQTT button_press

The panel publishes **every** tap (light, fan, climate, room mode) to **`esp_hmi/device/<MAC>/status/button_press`** as JSON **`{"button": "<id>"}`** (QoS 0 for controls, QoS 1 for room). See [`home_assistant/ha_automation_button_press.yaml`](home_assistant/ha_automation_button_press.yaml) (wildcard `esp_hmi/device/+/status/button_press`). **Migration:** Older builds used `esp_hmi/device/+/button_press`; Home Assistant MQTT triggers must be updated or they will never fire.

On each MQTT connect the device also publishes **retained** JSON to **`esp_hmi/device/<MAC>/status/parameters`** (QoS 1) with firmware/version and identity fields (`node_id`, `mac`, `firmware_version`, `chip_target`, `chip_revision`, `ha_device_name`, `wifi_ssid` or null, `default_screen`, `idle_timeout_screen`, `idle_timeout_seconds`, embedded `compile_date`/`compile_time`, etc.). Do not manually retain conflicting payloads on this topic—the device overwrites its own retained message on reconnect.

To **push per-panel idle defaults when that heartbeat arrives**, use [`home_assistant_automations/ha_automation_hmi_defaults_from_parameters.yaml`](home_assistant_automations/ha_automation_hmi_defaults_from_parameters.yaml): it listens on `esp_hmi/device/+/status/parameters` and publishes **`cmd/set_idle_timeout`** (JSON) per MAC. **`cmd/set_default_screen`** examples in that file are **commented out by default** so they do not overwrite the HA **Default screen** select on every parameters update; uncomment only if you want HA to enforce a fixed slug. Extend the `choose` branches for additional devices.

For **idle timeout** defaults over the same heartbeat, see [`home_assistant/ha_automation_hmi_idle_from_parameters.yaml`](home_assistant/ha_automation_hmi_idle_from_parameters.yaml) (publishes **`cmd/set_idle_timeout`** JSON per device).

**Light / fan** use `remote.send_command` (same remote as before). Replace `device_id` with your remote’s id (Developer Tools → **Devices**). Adjust `device: rf_fan` if your integration expects a different key.

**Climate** −/+, in [`home_assistant/ha_automation_button_press.yaml`](home_assistant/ha_automation_button_press.yaml), calls **`climate.set_temperature`** on **`climate.bedroom_3_heater_bedroom_3_climate_control`** (variable `climate_target_entity`, **0.5°** per tap). Mode buttons use **`switch.bedroom_3_heater`** and **`climate.set_hvac_mode`** (`off` vs `heat`).

| `trigger.payload_json.button` | Action |
| ----------------------------- | ------ |
| `light` | `remote.send_command` → `light_toggle` |
| `fan_1` | `remote.send_command` → `fan_low` |
| `fan_2` | `remote.send_command` → `fan_medium` |
| `fan_3` | `remote.send_command` → `fan_high` |
| `fan_off` | `remote.send_command` → `fan_off` |
| `climate_temp_dn` | `climate.set_temperature` on `climate.bedroom_3_heater_bedroom_3_climate_control` (−0.5°) |
| `climate_temp_up` | same climate entity (+0.5°) |
| `climate_mode_off` | `switch.turn_off` `switch.bedroom_3_heater`; `climate.set_hvac_mode` → `off` on `climate.bedroom_3_heater_bedroom_3_climate_control` |
| `climate_mode_on` | `switch.turn_on` `switch.bedroom_3_heater`; `climate.set_hvac_mode` → `off` on that climate entity |
| `climate_mode_cc` | `climate.set_hvac_mode` → `heat` on that climate entity |
| `study_heater` | `switch.toggle` → `switch.tasmota_2` |

**Automation — light, fan, climate** (same `status/button_press` topic as room; exclude room option strings so the room automation handles them). Copy-ready YAML: [`home_assistant/ha_automation_button_press.yaml`](home_assistant/ha_automation_button_press.yaml).

**Merging into Home Assistant:** [`home_assistant/ha_automation_data_bedroom_3_climate.yaml`](home_assistant/ha_automation_data_bedroom_3_climate.yaml), [`home_assistant/ha_automation_data_bedroom_3_mode.yaml`](home_assistant/ha_automation_data_bedroom_3_mode.yaml), [`home_assistant/ha_automation_data_study_heater.yaml`](home_assistant/ha_automation_data_study_heater.yaml), [`home_assistant/ha_automation_button_press.yaml`](home_assistant/ha_automation_button_press.yaml), [`home_assistant/ha_automation_hmi_defaults_from_parameters.yaml`](home_assistant/ha_automation_hmi_defaults_from_parameters.yaml), and [`home_assistant/ha_automation_hmi_idle_from_parameters.yaml`](home_assistant/ha_automation_hmi_idle_from_parameters.yaml) are each a **single automation mapping** — paste into **Create automation → Edit in YAML**, or append into `automations.yaml`. Do not wrap file contents in an extra `automation:` key when appending to `automations.yaml`. If you paste a **YAML list** of multiple `- alias:` automations into the single-automation editor, Home Assistant may reject it with **`Message malformed: extra keys not allowed @ data['0']`**.

These snippets use the **current** automation keys **`triggers:`** / **`actions:`** and **`action:`** (not legacy `trigger:` / `action:` / `service:`). If you still see **Message malformed: extra keys not allowed @ data['0']`**, you are usually pasting **old-format** YAML into an editor that only accepts the new shape—re-copy from the YAML files under [`home_assistant/`](home_assistant/) in this repo.

For any automation whose **MQTT trigger `topic:`** is a template that includes the MAC (e.g. `esp_hmi/device/{{ … }}/status/button_press`), put the MAC in **`trigger_variables:`** (e.g. `hmi_mac_hex`). Regular **`variables:`** are not available to that limited template, so the broker subscription would not match and the automation would never run. **`variables:`** is still used for values only referenced from **conditions** / **actions** (e.g. `climate_ollie_room`, or `hmi_mac_hex` when it appears only in `mqtt.publish` data).

Climate **+/−** in [`home_assistant/ha_automation_button_press.yaml`](home_assistant/ha_automation_button_press.yaml) sets **`hvac_mode: heat`** with each `climate.set_temperature` call (matches the [climate docs](https://www.home-assistant.io/integrations/climate/#action-set-temperature) example). Remove those two `hvac_mode` lines if your thermostat should stay in another mode.

Study heater state for the Study page is mirrored from Home Assistant over retained MQTT topic **`esp_hmi/data/study/heater_on`** using [`home_assistant/ha_automation_data_study_heater.yaml`](home_assistant/ha_automation_data_study_heater.yaml). Payload is `switch.tasmota_2` state (`on`/`off`), and firmware accepts bool-like scalars (`true`/`false`/`on`/`off`/`1`/`0`).

## Firmware version

The three-part version is the source of truth in the firmware root [`panel_firmware/CMakeLists.txt`](panel_firmware/CMakeLists.txt): **`FW_VER_MAJOR`**, **`FW_VER_MINOR`**, **`FW_VER_PATCH`** (integers). They are passed into the `main` component as compile definitions; the UI formats them (e.g. `1.0.0`). **`PROJECT_VER`** is derived from the same triple for `esp_app_desc` / OTA alignment. Bump those three variables when you release.

## Default startup screen (NVS + MQTT)

After the loading splash, the UI loads the screen id stored in NVS (**namespace** `esp_hmi`, **key** `def_app`, `uint8_t` / `app_id_t`).

**Idle return screen** (optional): **keys** `idle_app` (`uint8_t`) and `idle_sec` (`uint32_t`). If `idle_sec` is **0**, the feature is off. Otherwise, after no **user activity** for that many seconds on a normal app screen, the UI navigates to `idle_app` (loading / OTA UI are excluded). **Activity** includes LVGL pointer input (touch, gestures) and **any change to a registered app screen**—including `nav_go_to` from MQTT (`switch_screen`, idle return, temporary switch restore, etc.)—which resets the idle countdown. Values are set over MQTT (see below) and mirrored in `status/parameters`.

On MQTT connect the device **subscribes** (QoS **1**) to:

**`esp_hmi/device/<aabbccddeeff>/cmd/set_default_screen`**

where `<aabbccddeeff>` is the station MAC as **12 lowercase hex digits** (same token as in `esp_hmi/device/<mac>/status/button_press`). Payload is **plain text**, trimmed of whitespace, one of:

`home` · `ollie_room` · `dashboard` · `front_gate` · `pipboy` · `settings` · `study`

Publishing a **retained** message is supported so panels pick up the default after reconnect.

## MQTT: switch screen now, temporarily, or reboot

Uses the **same `<aabbccddeeff>`** device id as **`set_default_screen`**. Subscribe on connect is QoS **1**. Prefer **non-retained** messages (otherwise they refire after reconnect).

| Topic | Payload | Behaviour |
| ----- | ------- | --------- |
| `esp_hmi/device/<mac>/cmd/switch_screen` | JSON **`{"screen":"…"}`** (UTF-8). `screen` is the same slug as startup default (`home`, `ollie_room`, …). | Navigate **immediately**; does **not** change NVS default. Cancels any in-progress temporary switch timer. |
| `esp_hmi/device/<mac>/cmd/switch_screen_temp` | JSON (UTF-8) | Show target screen **only for a duration**, then return to whichever screen was active when the message was handled. Cancels any previous temporary timer first. Target: **`screen`** (preferred) or **`slug`** — same slug strings as above. Exactly one duration field: **`seconds`** *or* **`duration_ms`** (number). Implicit clamp **250 ms … 86400000 ms** (~24 h). |
| `esp_hmi/device/<mac>/cmd/set_idle_timeout` | JSON **`{"screen":"…","seconds":<n>}`** (UTF-8). **`seconds`** `0` **disables** idle return (target still stored in NVS). Applied on the LVGL thread; **persists** to NVS. | After **no touch/gesture and no registered screen change** for **`seconds`**, load **`screen`**. Navigating (swipe, MQTT `switch_screen`, temp restore, …) resets the timer. Does not run on loading or OTA progress screens. |
| `esp_hmi/device/<mac>/cmd/reboot` | Ignored (`1`, empty JSON, …) | **Reboot** after ~250 ms deferral (`esp_restart()`), scheduled from LVGL flush context via a tiny FreeRTOS task. |

**Gestures/tiles**: Any normal `nav_go_to` / swipe navigation **cancels** the temporary-screen timer without firing the auto-restore step.

**Examples:**

```bash
mosquitto_pub -h <broker> -t 'esp_hmi/device/<mac>/cmd/switch_screen' \
  -m '{"screen":"front_gate"}' -q 1

mosquitto_pub -h <broker> -t 'esp_hmi/device/<mac>/cmd/switch_screen_temp' \
  -m '{"screen":"front_gate","seconds":15}' -q 1

mosquitto_pub -h <broker> -t 'esp_hmi/device/<mac>/cmd/set_idle_timeout' \
  -m '{"screen":"home","seconds":120}' -q 1
```

## OTA firmware update (HTTPS, MQTT command)

The partition table uses two OTA app slots (`ota_0` / `ota_1`, see [`panel_firmware/partitions.csv`](panel_firmware/partitions.csv)) with **rollback** support (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` in [`panel_firmware/sdkconfig.defaults`](panel_firmware/sdkconfig.defaults)). On boot, a successful app run calls `esp_ota_mark_app_valid_cancel_rollback()` so the bootloader keeps the new firmware.

**Subscribe topic** (QoS **1**, suffix configurable in menu **Screen test 1 — OTA**):

**`esp_hmi/device/<aabbccddeeff>/<suffix>`** — default suffix **`cmd/ota_update`** → full topic example: `esp_hmi/device/30eda0e20a9e/cmd/ota_update`.

**Payload** (JSON, UTF-8):

| Field | Required | Meaning |
| ----- | -------- | ------- |
| `url` | yes | HTTPS URL to the **`.bin`** firmware image |
| `version` | no | If it matches the running `esp_app_desc` version string (same as `PROJECT_VER`), the update is **skipped** |
| `sha256` | no\* | 64 hex chars, SHA-256 of the **first `size` bytes** of the `.bin` file |
| `size` | no\* | Byte length to hash (must match `.bin` size used for `sha256`) |

\*If you include `sha256`, you must also include `size`; otherwise omit both.

**Retain:** Prefer **not** retaining OTA commands (one-shot). If a retained OTA message sits on the broker, the device may try to update again on every reconnect.

**Example (`mosquitto_pub`):**

```bash
mosquitto_pub -h <broker> -t 'esp_hmi/device/aabbccddeeff/cmd/ota_update' -m '{"url":"https://example.com/screen_test_1.bin","version":"1.1.0"}' -q 1
```

**Example (Home Assistant action):**

```yaml
action: mqtt.publish
data:
  topic: esp_hmi/device/aabbccddeeff/cmd/ota_update
  payload: '{"url":"https://example.com/screen_test_1.bin","version":"1.1.0"}'
  qos: 1
```

**Security:** Public HTTPS hosts should use the default CA bundle (`SCREEN_TEST_OTA_SKIP_CERT_VERIFY` off). For LAN / self-signed firmware servers, enable **Screen test 1 — OTA → Skip HTTPS server certificate verification** and ensure your global TLS settings match (this repo’s defaults already allow insecure TLS for other HTTP clients—still treat OTA URLs as trusted inputs).

**First migration from factory-only layout:** After changing [`panel_firmware/partitions.csv`](panel_firmware/partitions.csv), do a **full chip erase** or flash with erase before the first OTA-capable image so partition metadata is consistent.

## Configuration

- **Wi‑Fi / MQTT / Home Assistant**: Menu **Screen test 1 — Network / Home Assistant** (`panel_firmware/main/Kconfig.projbuild`). Set **WiFi SSID** (required for MQTT; leave empty to skip Wi‑Fi/MQTT and keep UI-only). Defaults: broker `mqtt://192.168.1.52:1883`, MQTT user `mqttdevice`, device name `ESP32-P4 Ollie Room`. On **ESP32-P4**, Wi‑Fi runs on the onboard **ESP32‑C6** via **esp_hosted** (SDIO) and **esp_wifi_remote**; [`panel_firmware/sdkconfig.defaults`](panel_firmware/sdkconfig.defaults) disables `CONFIG_ESP_HOST_WIFI_ENABLED` (mutually exclusive with remote Wi‑Fi) and selects the Espressif **ESP32‑P4 Function EV** SDIO pin preset with **C6** slave target. **Do not commit real Wi‑Fi or MQTT secrets** in a public repo—override in local `sdkconfig` or clear Kconfig defaults before pushing.
- **Room mode MQTT topics** (same menu): **HA → panels** use **`SCREEN_TEST_MQTT_ROOM_STATE_TOPIC`** (default `esp_hmi/data/bedroom3/desired_state`, retained plain-text: `Normal`, `Rest Time`, or `Sleep Time`). Override with **`SCREEN_TEST_MQTT_ROOM_STATE_TOPIC_OVERRIDE`** if needed. **Panels → HA** room taps **publish** **`{"button": "…"}`** on **`esp_hmi/device/<MAC>/status/button_press`**. Optional **`SCREEN_TEST_MQTT_ROOM_SET_TOPIC_OVERRIDE`**: if set, that JSON is published to that topic instead of `status/button_press`.
- **Climate MQTT topics** (same menu): Four full topic strings (`SCREEN_TEST_MQTT_CLIMATE_SETPOINT_TOPIC`, `…_CURRENT_TOPIC`, `…_HEATER_TOPIC`, `…_CONTROL_TOPIC`), defaults under `esp_hmi/data/bedroom3/climate/`. Plain scalar payloads; see [Ollie climate](#home-assistant-ollie-climate--mqtt) and [`panel_firmware/docs/asyncapi.yaml`](panel_firmware/docs/asyncapi.yaml).
- **Lottie / ThorVG**: Enabled in [`panel_firmware/sdkconfig.defaults`](panel_firmware/sdkconfig.defaults) (`CONFIG_LV_USE_VECTOR_GRAPHIC`, `CONFIG_LV_USE_THORVG`, `CONFIG_LV_USE_THORVG_INTERNAL`, `CONFIG_LV_USE_LOTTIE`, plus `CONFIG_LV_FONT_MONTSERRAT_24` for symbols). Equivalent path in menuconfig: **Component config → LVGL** (vector graphics, ThorVG, Lottie). **Gesture navigation** requires `CONFIG_LV_USE_FLOAT` and `CONFIG_LV_USE_GESTURE_RECOGNITION` (also set in defaults).
- **Loading duration**: **Screen test 1 UI → Loading screen duration (ms)** (`CONFIG_SCREEN_TEST_LOADING_MS`, default 3000), defined in [`panel_firmware/main/Kconfig.projbuild`](panel_firmware/main/Kconfig.projbuild).
- **Flash / partitions**: The firmware is ~1.2 MiB; the project uses a **custom** [`panel_firmware/partitions.csv`](panel_firmware/partitions.csv) with **two ~2.5 MiB OTA app slots** (`ota_0` / `ota_1`), **otadata**, and **16 MiB** flash in defaults (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB`). If you already have a local `sdkconfig` from an older profile, run `idf.py menuconfig` and align **Serial flasher → Flash size** and **Partition Table** with `panel_firmware/sdkconfig.defaults`, or remove `sdkconfig` and run `idf.py set-target esp32p4` again so defaults apply cleanly.
- **OTA (MQTT / HTTPS)**: Menu **Screen test 1 — OTA** (`panel_firmware/main/Kconfig.projbuild`). Enable **`SCREEN_TEST_OTA_ENABLE`**, set **`SCREEN_TEST_OTA_MQTT_CMD_SUFFIX`** (default `cmd/ota_update`), timeout, and optional skip-verify for self-signed URLs. See [OTA firmware update](#ota-firmware-update-https-mqtt-command).
- **LVGL task stack (Lottie / ThorVG)**: [`panel_firmware/main/main.c`](panel_firmware/main/main.c) starts the display with **`bsp_display_start_with_config`** and sets **`task_stack` to 32 KiB**. The BSP default from `ESP_LVGL_PORT_INIT_CONFIG()` is only **7168** bytes; ThorVG’s software renderer uses enough stack that a smaller value triggers a **stack protection fault** in `taskLVGL` (often reported inside `tvgSwRle.cpp`).
- **Main task stack / Lottie thread**: LVGL’s `lv_lottie_set_src_data()` calls **`lottie_update()` immediately** on the calling thread. [`panel_firmware/main/main.c`](panel_firmware/main/main.c) defers **`ui_shell_init()`** with **`lv_async_call()`** so that draw uses **`taskLVGL`’s** stack. [`panel_firmware/sdkconfig.defaults`](panel_firmware/sdkconfig.defaults) still sets **`CONFIG_ESP_MAIN_TASK_STACK_SIZE=32768`** as a safety margin if your `sdkconfig` picks it up (an older `sdkconfig` may keep the default ~4 KiB until you reconfigure).

## Swapping the startup Lottie

Replace [`panel_firmware/main/assets/lottie_startup.json`](panel_firmware/main/assets/lottie_startup.json) with any valid Lottie JSON for ThorVG, then rebuild. The file is embedded via `EMBED_FILES` in [`panel_firmware/main/CMakeLists.txt`](panel_firmware/main/CMakeLists.txt); the linker exposes `_binary_lottie_startup_json_start` / `_binary_lottie_startup_json_end` (see [`panel_firmware/main/ui/screens/screen_loading.c`](panel_firmware/main/ui/screens/screen_loading.c)). Use a **moderate** render size (this project uses a **128×128** ARGB buffer) so RAM and CPU stay reasonable. **Heavy** Lottie scenes (large composition size, wide strokes, many layers) can make ThorVG spend seconds inside one frame and trigger the **task watchdog** (IDLE starved). Prefer simple shapes; the checked-in placeholder is intentionally minimal.

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) **v5.3.1 or newer** (see Waveshare wiki for [**ESP32-P4-WIFI6-Touch-LCD-3.4C**](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-3.4C) or [**ESP32-P4-WIFI6-Touch-LCD-4B**](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B))
- USB on **USB TO UART**
- **PSRAM** — MIPI-DPI frame buffers use `MALLOC_CAP_SPIRAM`. This project sets hex PSRAM at 200 MHz in [`panel_firmware/sdkconfig.defaults`](panel_firmware/sdkconfig.defaults).

## Build and flash

From `panel_firmware/`:

```bash
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

On first build, ESP-IDF resolves the [Waveshare BSP **`waveshare/esp32_p4_wifi6_touch_lcd_4b`**](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_4b) into `panel_firmware/managed_components/` (same package for **3.4C** and **4B** 720×720 round boards; see [Waveshare round boards](#waveshare-round-boards-34c-and-4b) above).

### Flash fails: `Serial data stream stopped` during “Uploading stub”

`idf.py` defaults to **460800** baud for flashing. On some USB cables, hubs, or macOS serial stacks the link can corrupt during stub upload even though chip detection succeeds.

1. Flash at a lower baud (recommended first step):

   ```bash
   idf.py -p PORT -b 115200 flash monitor
   ```

   Or set the environment variable used by `idf.py` (see ESP-IDF `serial_ext.py`): `export ESPBAUD=115200` then run `idf.py flash` as usual.

2. Ensure no other program has the serial port open (another monitor, Arduino IDE, Bluetooth serial helpers).

3. Try a different USB cable (data-capable), a direct Mac port instead of a hub, and the board’s **USB TO UART** port only.

4. If it still fails at stub upload, enable **Serial flasher config → Disable download stub** in `menuconfig` (sets `CONFIG_ESPTOOLPY_NO_STUB`), then build and flash again (slower, but avoids the stub transfer).

## ESP32-C6 co-processor (esp_hosted slave)

The C6 is **not** flashed with this project’s `screen_test_1.bin`. It must run **Espressif esp_hosted slave** firmware whose version matches the **esp_hosted** component in [`panel_firmware/main/idf_component.yml`](panel_firmware/main/idf_component.yml) / [`panel_firmware/dependencies.lock`](panel_firmware/dependencies.lock). Boards often ship with a compatible image already.

If Wi‑Fi never associates after the host changes (no `got IP`, transport errors in the log):

1. Confirm the **host** `sdkconfig` matches your board wiring; the default in [`panel_firmware/sdkconfig.defaults`](panel_firmware/sdkconfig.defaults) is `CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD` (Espressif Function EV + C6 SDIO). **3.4C** and **4B** schematics may differ—if Wi‑Fi or transport fails, adjust **Component config → ESP-Hosted** SDIO GPIOs per the [3.4C wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-3.4C) or [4B wiki](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B).
2. Build or download the **C6 slave** from the [esp_hosted](https://github.com/espressif/esp-hosted) repo for target **esp32c6** and the same release as the host component, then flash the C6 via the board’s **UART** pads (see Waveshare wiki / schematic for `C6` download wiring).

## UI layout (source tree)

Screens and widgets are **one `.c` / `.h` pair per module** under `panel_firmware/main/ui/` (`ui_shell`, `nav`, `screens/*`, `components/*`). Shared layout helpers: include [`panel_firmware/main/ui/ui_layout.h`](panel_firmware/main/ui/ui_layout.h) from any screen or component (currently re-exports **square grid** math in [`ui_layout_grid.c`](panel_firmware/main/ui/ui_layout_grid.c) / [`ui_layout_grid.h`](panel_firmware/main/ui/ui_layout_grid.h): gap, stride, cell → x/y). Entry point: `ui_shell_init()` from [`panel_firmware/main/main.c`](panel_firmware/main/main.c).
