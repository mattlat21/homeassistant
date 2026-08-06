"""Constants for the ESP HMI Panels integration."""

DOMAIN = "esp_hmi"

CONF_TOPIC_PREFIX = "topic_prefix"
DEFAULT_TOPIC_PREFIX = "esp_hmi"

DATA_RUNTIME = "runtime"

PLATFORMS = ["sensor", "button", "select", "number", "binary_sensor"]

# Max seconds for the idle-timeout number entity (firmware may clamp higher on device).
IDLE_TIMEOUT_SECONDS_MAX = 31_536_000  # 365 days

# Max seconds for display dim/off timeout number entities (matches firmware clamp).
DISPLAY_TIMEOUT_SECONDS_MAX = 86_400

# Max seconds for brightness fade duration (0 = instant).
DISPLAY_FADE_SECONDS_MAX = 60

# Display brightness range (percent).
DISPLAY_BRIGHTNESS_MIN = 0
DISPLAY_BRIGHTNESS_MAX = 100

# Panel screen slugs (must match firmware app_prefs slug list)
SCREEN_OPTIONS: tuple[str, ...] = (
    "home",
    "ollie_room",
    "dashboard",
    "front_gate",
    "pipboy",
    "settings",
    "study",
    "about",
    "front_door",
    "kitchen",
    "studio",
    "hvac",
    "house_battery",
    "penny_room",
)

# Service names
SERVICE_SWITCH_SCREEN = "switch_screen"
SERVICE_SWITCH_SCREEN_TEMP = "switch_screen_temp"
SERVICE_SET_DEFAULT_SCREEN = "set_default_screen"
SERVICE_SET_IDLE_TIMEOUT = "set_idle_timeout"
SERVICE_SET_DISPLAY_POWER = "set_display_power"
SERVICE_WAKE_DISPLAY = "wake_display"
SERVICE_REBOOT = "reboot"

# Remote OTA (must match firmware CONFIG_ESP_HMI_OTA_MQTT_CMD_SUFFIX)
OTA_CMD_SUFFIX = "cmd/ota_update"

# Published firmware builds (add new versions when binaries are staged/uploaded)
FIRMWARE_VERSION_OPTIONS: tuple[str, ...] = ("1.1.2", "1.1.3", "1.1.4", "1.1.5")

# OTA image URL: {base}/v{version}/esp_hmi.bin
FIRMWARE_OTA_URL_BASE = "http://latimer.net/ha/fware/esphmi"

# Internal event name used by device_trigger.py
EVENT_BUTTON_PRESS = "esp_hmi_button_press"

