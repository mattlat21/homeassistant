"""ESP HMI Panels integration."""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
import json
import logging
from typing import Any

import voluptuous as vol

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, ServiceCall, callback
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers.dispatcher import async_dispatcher_send
from homeassistant.helpers.typing import ConfigType

from homeassistant.components import mqtt

from .const import (
    CONF_TOPIC_PREFIX,
    DATA_RUNTIME,
    DEFAULT_TOPIC_PREFIX,
    DOMAIN,
    EVENT_BUTTON_PRESS,
    PLATFORMS,
    SERVICE_REBOOT,
    SERVICE_SET_DEFAULT_SCREEN,
    SERVICE_SET_IDLE_TIMEOUT,
    SERVICE_SWITCH_SCREEN,
    SERVICE_SWITCH_SCREEN_TEMP,
)

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = vol.Schema(
    {
        DOMAIN: vol.Schema(
            {
                vol.Optional(CONF_TOPIC_PREFIX, default=DEFAULT_TOPIC_PREFIX): str,
            }
        )
    },
    extra=vol.ALLOW_EXTRA,
)

SIGNAL_NEW_DEVICE = f"{DOMAIN}_new_device"
SIGNAL_PARAMETERS_UPDATE = f"{DOMAIN}_parameters_update"
SIGNAL_STATUS_UPDATE = f"{DOMAIN}_status_update"


def _now_utc() -> datetime:
    return datetime.now(timezone.utc)


def _mac_is_reasonable(mac: str) -> bool:
    mac = mac.strip().lower()
    return len(mac) == 12 and all(c in "0123456789abcdef" for c in mac)


def _topic_parts(topic: str) -> list[str]:
    return [p for p in topic.split("/") if p]


def _mac_from_topic(prefix: str, topic: str) -> str | None:
    # Expected: <prefix>/device/<mac>/status/...
    parts = _topic_parts(topic)
    expected_prefix = _topic_parts(prefix)
    if parts[: len(expected_prefix)] != expected_prefix:
        return None
    # after prefix, want: device/<mac>/...
    if len(parts) < len(expected_prefix) + 3:
        return None
    if parts[len(expected_prefix)] != "device":
        return None
    mac = parts[len(expected_prefix) + 1].lower()
    return mac if _mac_is_reasonable(mac) else None


@dataclass
class PanelState:
    mac: str
    parameters: dict[str, Any] = field(default_factory=dict)
    parameters_updated_at: datetime | None = None
    seen_buttons: set[str] = field(default_factory=set)
    #: Last screen slug sent via `cmd/switch_screen` from HA (optional UI hint).
    last_remote_nav_screen: str | None = None
    #: From retained `status/current_screen` (plain slug or `unknown`).
    current_screen: str | None = None
    #: From retained `status/mqtt_connected` (`ON` / `OFF`).
    mqtt_connected: bool | None = None


@dataclass
class EspHmiRuntime:
    topic_prefix: str
    panels: dict[str, PanelState] = field(default_factory=dict)
    unsubscribers: list[callable] = field(default_factory=list)

    def get_or_create_panel(self, mac: str) -> PanelState:
        if mac not in self.panels:
            self.panels[mac] = PanelState(mac=mac)
        return self.panels[mac]


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """Set up from YAML (optional/legacy)."""
    # YAML fallback: import into a config entry if no entries exist.
    if DOMAIN in config and not hass.config_entries.async_entries(DOMAIN):
        data = config[DOMAIN] or {}
        await hass.config_entries.flow.async_init(
            DOMAIN, context={"source": "import"}, data=data
        )
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up ESP HMI from a config entry."""
    topic_prefix = entry.data.get(CONF_TOPIC_PREFIX, DEFAULT_TOPIC_PREFIX).strip("/")
    runtime = EspHmiRuntime(topic_prefix=topic_prefix)

    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN][entry.entry_id] = {DATA_RUNTIME: runtime}

    device_registry = dr.async_get(hass)

    @callback
    def _ensure_device(mac: str, params: dict[str, Any]) -> None:
        identifiers = {(DOMAIN, mac)}
        # These are best-effort; params keys are from panel firmware.
        name = params.get("ha_device_name") or f"ESP HMI {mac}"
        sw_version = params.get("firmware_version")
        model = params.get("chip_target") or "ESP32 panel"
        manufacturer = "Waveshare"

        device_registry.async_get_or_create(
            config_entry_id=entry.entry_id,
            identifiers=identifiers,
            name=name,
            manufacturer=manufacturer,
            model=model,
            sw_version=sw_version,
        )

    async def _handle_parameters(msg: mqtt.ReceiveMessage) -> None:
        mac = _mac_from_topic(topic_prefix, msg.topic)
        if not mac:
            return
        try:
            payload = json.loads(msg.payload)
        except Exception:  # noqa: BLE001
            _LOGGER.debug("Invalid parameters JSON for %s: %s", msg.topic, msg.payload)
            return
        if not isinstance(payload, dict):
            return

        panel = runtime.get_or_create_panel(mac)
        panel.parameters = payload
        panel.parameters_updated_at = _now_utc()
        _ensure_device(mac, payload)

        # Inform platforms: create entities if needed, then update.
        async_dispatcher_send(hass, SIGNAL_NEW_DEVICE, entry.entry_id, mac)
        async_dispatcher_send(hass, SIGNAL_PARAMETERS_UPDATE, entry.entry_id, mac)

    def _payload_text(msg: mqtt.ReceiveMessage) -> str:
        raw = msg.payload
        if isinstance(raw, bytes):
            return raw.decode("utf-8", errors="replace").strip()
        return str(raw).strip()

    async def _handle_current_screen(msg: mqtt.ReceiveMessage) -> None:
        mac = _mac_from_topic(topic_prefix, msg.topic)
        if not mac:
            return
        text = _payload_text(msg)
        panel = runtime.get_or_create_panel(mac)
        panel.current_screen = text if text else None
        async_dispatcher_send(hass, SIGNAL_STATUS_UPDATE, entry.entry_id, mac)

    async def _handle_mqtt_connected(msg: mqtt.ReceiveMessage) -> None:
        mac = _mac_from_topic(topic_prefix, msg.topic)
        if not mac:
            return
        raw_up = _payload_text(msg).upper()
        panel = runtime.get_or_create_panel(mac)
        if raw_up == "ON":
            panel.mqtt_connected = True
        elif raw_up == "OFF":
            panel.mqtt_connected = False
        else:
            panel.mqtt_connected = None
        async_dispatcher_send(hass, SIGNAL_STATUS_UPDATE, entry.entry_id, mac)

    async def _handle_button_press(msg: mqtt.ReceiveMessage) -> None:
        mac = _mac_from_topic(topic_prefix, msg.topic)
        if not mac:
            return
        try:
            payload = json.loads(msg.payload)
        except Exception:  # noqa: BLE001
            return
        if not isinstance(payload, dict):
            return
        button = payload.get("button")
        if not isinstance(button, str) or not button:
            return

        panel = runtime.get_or_create_panel(mac)
        panel.seen_buttons.add(button)

        hass.bus.async_fire(
            EVENT_BUTTON_PRESS,
            {
                "entry_id": entry.entry_id,
                "mac": mac,
                "button": button,
                "raw": payload,
                "topic": msg.topic,
            },
        )

    async def _publish_cmd(mac: str, suffix: str, payload: str, qos: int = 1) -> None:
        await mqtt.async_publish(
            hass,
            f"{topic_prefix}/device/{mac}/{suffix}",
            payload=payload,
            qos=qos,
            retain=False,
        )

    async def _svc_switch_screen(call: ServiceCall) -> None:
        mac = str(call.data["mac"]).lower()
        screen = str(call.data["screen"])
        await _publish_cmd(mac, "cmd/switch_screen", json.dumps({"screen": screen}), qos=1)

    async def _svc_switch_screen_temp(call: ServiceCall) -> None:
        mac = str(call.data["mac"]).lower()
        screen = str(call.data["screen"])
        seconds = int(call.data["seconds"])
        await _publish_cmd(
            mac,
            "cmd/switch_screen_temp",
            json.dumps({"screen": screen, "seconds": seconds}),
            qos=1,
        )

    async def _svc_set_default_screen(call: ServiceCall) -> None:
        mac = str(call.data["mac"]).lower()
        screen = str(call.data["screen"])
        await _publish_cmd(mac, "cmd/set_default_screen", screen, qos=1)

    async def _svc_set_idle_timeout(call: ServiceCall) -> None:
        mac = str(call.data["mac"]).lower()
        screen = str(call.data["screen"])
        seconds = int(call.data["seconds"])
        await _publish_cmd(
            mac,
            "cmd/set_idle_timeout",
            json.dumps({"screen": screen, "seconds": seconds}),
            qos=1,
        )

    async def _svc_reboot(call: ServiceCall) -> None:
        mac = str(call.data["mac"]).lower()
        await _publish_cmd(mac, "cmd/reboot", "1", qos=1)

    # Register services once per config entry (namespaced under the integration domain).
    hass.services.async_register(DOMAIN, SERVICE_SWITCH_SCREEN, _svc_switch_screen)
    hass.services.async_register(DOMAIN, SERVICE_SWITCH_SCREEN_TEMP, _svc_switch_screen_temp)
    hass.services.async_register(DOMAIN, SERVICE_SET_DEFAULT_SCREEN, _svc_set_default_screen)
    hass.services.async_register(DOMAIN, SERVICE_SET_IDLE_TIMEOUT, _svc_set_idle_timeout)
    hass.services.async_register(DOMAIN, SERVICE_REBOOT, _svc_reboot)

    # Subscribe to the two main firmware topics (see README.md).
    runtime.unsubscribers.append(
        await mqtt.async_subscribe(
            hass,
            f"{topic_prefix}/device/+/status/parameters",
            _handle_parameters,
            qos=1,
        )
    )
    runtime.unsubscribers.append(
        await mqtt.async_subscribe(
            hass,
            f"{topic_prefix}/device/+/status/button_press",
            _handle_button_press,
            qos=0,
        )
    )
    runtime.unsubscribers.append(
        await mqtt.async_subscribe(
            hass,
            f"{topic_prefix}/device/+/status/current_screen",
            _handle_current_screen,
            qos=1,
        )
    )
    runtime.unsubscribers.append(
        await mqtt.async_subscribe(
            hass,
            f"{topic_prefix}/device/+/status/mqtt_connected",
            _handle_mqtt_connected,
            qos=1,
        )
    )

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    unload_ok = await hass.config_entries.async_unload_platforms(entry, PLATFORMS)
    if not unload_ok:
        return False

    runtime: EspHmiRuntime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]
    for unsub in runtime.unsubscribers:
        try:
            unsub()
        except Exception:  # noqa: BLE001
            pass

    # Services are global; for now we leave them registered. (If you add multi-entry support,
    # switch to a reference-counted register/unregister pattern.)

    hass.data[DOMAIN].pop(entry.entry_id, None)
    if not hass.data[DOMAIN]:
        hass.data.pop(DOMAIN, None)
    return True

