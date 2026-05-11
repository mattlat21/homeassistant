"""Device triggers for ESP HMI Panels button presses."""

from __future__ import annotations

from collections.abc import Callable
import logging
from typing import Any

import voluptuous as vol

from homeassistant.const import CONF_DEVICE_ID, CONF_DOMAIN, CONF_PLATFORM, CONF_TYPE, CONF_SUBTYPE
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers import event as event_helper
from homeassistant.helpers.typing import ConfigType

from . import EspHmiRuntime
from .const import DATA_RUNTIME, DOMAIN, EVENT_BUTTON_PRESS

_LOGGER = logging.getLogger(__name__)

TRIGGER_TYPES = {"button_press"}

TRIGGER_SCHEMA = vol.Schema(
    {
        vol.Required(CONF_PLATFORM): "device",
        vol.Required(CONF_DOMAIN): DOMAIN,
        vol.Required(CONF_DEVICE_ID): str,
        vol.Required(CONF_TYPE): vol.In(TRIGGER_TYPES),
        vol.Required(CONF_SUBTYPE): str,
    }
)


async def async_get_triggers(hass: HomeAssistant, device_id: str) -> list[dict[str, Any]]:
    """List triggers for a device."""
    device_registry = dr.async_get(hass)
    device = device_registry.async_get(device_id)
    if device is None:
        return []

    # Try to find the MAC identifier for this device.
    mac: str | None = None
    for ident_domain, ident in device.identifiers:
        if ident_domain == DOMAIN:
            mac = ident
            break
    if mac is None:
        return []

    triggers: list[dict[str, Any]] = []

    # Aggregate seen buttons across all config entries (usually there’s one).
    runtimes: list[EspHmiRuntime] = []
    for entry_id_data in hass.data.get(DOMAIN, {}).values():
        runtime = entry_id_data.get(DATA_RUNTIME)
        if runtime is not None:
            runtimes.append(runtime)

    seen: set[str] = set()
    for runtime in runtimes:
        panel = runtime.panels.get(mac)
        if panel is not None:
            seen |= set(panel.seen_buttons)

    for button in sorted(seen):
        triggers.append(
            {
                CONF_PLATFORM: "device",
                CONF_DOMAIN: DOMAIN,
                CONF_DEVICE_ID: device_id,
                CONF_TYPE: "button_press",
                CONF_SUBTYPE: button,
            }
        )

    return triggers


async def async_attach_trigger(
    hass: HomeAssistant,
    config: ConfigType,
    action: Callable[[dict[str, Any]], Any],
    automation_info: dict[str, Any],
) -> Callable[[], None]:
    """Attach a trigger."""
    config = TRIGGER_SCHEMA(config)

    device_registry = dr.async_get(hass)
    device = device_registry.async_get(config[CONF_DEVICE_ID])
    if device is None:
        return lambda: None

    mac: str | None = None
    for ident_domain, ident in device.identifiers:
        if ident_domain == DOMAIN:
            mac = ident
            break
    if mac is None:
        return lambda: None

    subtype = config[CONF_SUBTYPE]

    @callback
    def _event_filter(event) -> bool:
        data = event.data
        return data.get("mac") == mac and data.get("button") == subtype

    return event_helper.async_track_event(
        hass,
        EVENT_BUTTON_PRESS,
        event_helper.async_handle_event(hass, automation_info, action),
        event_filter=_event_filter,
    )

