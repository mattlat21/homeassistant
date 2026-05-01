# Home Assistant

This repository is a **directory of projects** for Home Assistant–related configuration and firmware. Add new projects here as folders or top-level configs and extend this index when you add them.

## Projects

| Project | Description | Location |
|--------|---------------|----------|
| **Ceiling Alarm 1** | ESP32 (Adafruit Feather) device: servo positions (up, down, up rest), alarm cancel flow, SK6812 RGBW ring with spin effect and LED controls. | [`ESPHome YAML/ceiling-alarm-1.yaml`](ESPHome YAML/ceiling-alarm-1.yaml) |

## Secrets

ESPHome configs expect secrets (API key, OTA password, Wi‑Fi) in your ESPHome `secrets.yaml`; names referenced in the YAML include `ceiling-alarm-1-api-key`, `ceiling-alarm-1-ota-password`, `wifi_ssid`, and `wifi_password`.
