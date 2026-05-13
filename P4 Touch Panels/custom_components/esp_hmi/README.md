# ESP HMI Panels (custom integration)

This is a Home Assistant **custom integration** that turns ESP32-P4 touch panels running this repo’s firmware into first-class Home Assistant devices.

It is designed around the MQTT contract documented in the repo root [`README.md`](../../README.md):

- Panel publishes retained identity/parameters on: `esp_hmi/device/<mac>/status/parameters`
- Panel publishes button presses on: `esp_hmi/device/<mac>/status/button_press`
- Panel publishes retained live UI + link state on: `esp_hmi/device/<mac>/status/current_screen`, `.../status/mqtt_connected`
- Home Assistant can command the panel via `esp_hmi/device/<mac>/cmd/...`

## What you get (MVP)

- A **device** per panel (identified by MAC)
- **Sensors** from `status/parameters` (firmware version, SSID, default screen as read-only, chip info, etc.) plus **Current screen** (from `status/current_screen`)
- **Binary sensor** **MQTT connected** (from retained `status/mqtt_connected`; matches firmware MQTT discovery / LWT)
- **Select** entities: **Default screen**, **Idle timeout screen**, **Go to screen** (immediate `cmd/switch_screen` JSON — current UI only; does not change NVS default)
- **Number** entity: **Idle timeout seconds** (0 = disabled; publishes `cmd/set_idle_timeout` with the current idle screen slug)
- A **Reboot** button entity per panel
- **Device triggers** for button presses (so automations can be created in the UI)
- **Services** for the supported command topics:
  - `esp_hmi.switch_screen`
  - `esp_hmi.switch_screen_temp`
  - `esp_hmi.set_default_screen`
  - `esp_hmi.set_idle_timeout`
  - `esp_hmi.reboot`

## Install

Copy `custom_components/esp_hmi/` into your Home Assistant config folder:

`/config/custom_components/esp_hmi/`

Bundled **`icon.png`** and **`logo.png`** live next to `manifest.json`; Home Assistant picks them up for the integration card and settings UI. If you replace them, keep the same filenames.

### Samba (Home Assistant Yellow / “Samba share” add-on)

The helper script [`scripts/deploy_esp_hmi_to_ha.sh`](../../scripts/deploy_esp_hmi_to_ha.sh) copies files over SMB: it mounts the **`config`** share (same as `/config` on HA), wipes only `custom_components/esp_hmi/` on the share, then copies the local integration folder. Set credentials from the Samba add-on (**not** your SSH password unless they match). Easiest: use an env file (gitignored):

```bash
cp scripts/deploy_esp_hmi_to_ha.env.example scripts/deploy_esp_hmi_to_ha.env
# Edit SMB_USER / SMB_PASSWORD (use quotes for passwords with &, #, spaces)
./scripts/deploy_esp_hmi_to_ha.sh --dry-run
./scripts/deploy_esp_hmi_to_ha.sh
```

Or inline:

```bash
cd /path/to/P4\ Touch\ Panels
SMB_USER=your_samba_user SMB_PASSWORD='your_samba_password' ./scripts/deploy_esp_hmi_to_ha.sh --dry-run
SMB_USER=your_samba_user SMB_PASSWORD='your_samba_password' ./scripts/deploy_esp_hmi_to_ha.sh
```

`--restart` still uses **SSH** (`ha core restart`) if you want an automated restart after the copy; Samba alone cannot restart the core.

## Configure (preferred): UI config flow

1. Home Assistant → **Settings → Devices & Services → Add Integration**
2. Search for **ESP HMI Panels**
3. Set `topic_prefix` (default `esp_hmi`)

Panels will appear automatically as they publish `status/parameters`.

## Configure (advanced/legacy): YAML import

If you add this to `configuration.yaml` and restart, Home Assistant will import it into a config entry:

```yaml
esp_hmi:
  topic_prefix: esp_hmi
```

## Test quickly (manual MQTT)

1. Make sure your panel is connected to MQTT and publishes retained parameters.
2. Confirm you see retained data on the broker (example):
   - `esp_hmi/device/<mac>/status/parameters`
3. In Home Assistant, verify the device appears and sensors have values.

