"""Constants for the ESP HMI Panels integration."""

DOMAIN = "esp_hmi"

CONF_TOPIC_PREFIX = "topic_prefix"
DEFAULT_TOPIC_PREFIX = "esp_hmi"

DATA_RUNTIME = "runtime"

PLATFORMS = ["sensor", "button", "select", "number", "binary_sensor"]

# Max seconds for the idle-timeout number entity (firmware may clamp higher on device).
IDLE_TIMEOUT_SECONDS_MAX = 31_536_000  # 365 days

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
)

# Service names
SERVICE_SWITCH_SCREEN = "switch_screen"
SERVICE_SWITCH_SCREEN_TEMP = "switch_screen_temp"
SERVICE_SET_DEFAULT_SCREEN = "set_default_screen"
SERVICE_SET_IDLE_TIMEOUT = "set_idle_timeout"
SERVICE_REBOOT = "reboot"

# Internal event name used by device_trigger.py
EVENT_BUTTON_PRESS = "esp_hmi_button_press"

