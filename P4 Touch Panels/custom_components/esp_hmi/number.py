"""Number platform for ESP HMI Panels (idle timeout duration)."""

from __future__ import annotations

import json
from typing import Any

from homeassistant.components import mqtt
from homeassistant.components.number import NumberEntity, NumberMode
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.dispatcher import async_dispatcher_connect
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import EspHmiRuntime, SIGNAL_NEW_DEVICE, SIGNAL_PARAMETERS_UPDATE
from .const import (
    DATA_RUNTIME,
    DOMAIN,
    IDLE_TIMEOUT_SECONDS_MAX,
    SCREEN_OPTIONS,
)


def _coerce_uint_seconds(val: Any) -> int:
    if val is None:
        return 0
    if isinstance(val, bool):
        return int(val)
    if isinstance(val, (int, float)):
        return max(0, min(IDLE_TIMEOUT_SECONDS_MAX, int(val)))
    if isinstance(val, str):
        try:
            return max(0, min(IDLE_TIMEOUT_SECONDS_MAX, int(float(val))))
        except ValueError:
            return 0
    return 0


def _idle_screen_from_panel(panel: Any) -> str:
    if panel is None:
        return "home"
    val = panel.parameters.get("idle_timeout_screen")
    if isinstance(val, str) and val in SCREEN_OPTIONS:
        return val
    return "home"


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    runtime: EspHmiRuntime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]

    entities: dict[str, EspHmiIdleTimeoutSecondsNumber] = {}

    @callback
    def _maybe_add_for_mac(mac: str) -> None:
        if mac in entities:
            return
        ent = EspHmiIdleTimeoutSecondsNumber(entry.entry_id, runtime.topic_prefix, mac)
        entities[mac] = ent
        async_add_entities([ent])

    @callback
    def _on_new_device(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        _maybe_add_for_mac(mac)

    @callback
    def _on_parameters_update(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        ent = entities.get(mac)
        if ent is not None:
            ent.async_write_ha_state()

    for mac in list(runtime.panels):
        _maybe_add_for_mac(mac)

    entry.async_on_unload(async_dispatcher_connect(hass, SIGNAL_NEW_DEVICE, _on_new_device))
    entry.async_on_unload(
        async_dispatcher_connect(hass, SIGNAL_PARAMETERS_UPDATE, _on_parameters_update)
    )


class EspHmiIdleTimeoutSecondsNumber(NumberEntity):
    """Seconds until idle return (0 = disabled); uses cmd/set_idle_timeout JSON with current screen slug."""

    _attr_has_entity_name = True
    _attr_name = "Idle timeout seconds"
    _attr_native_min_value = 0
    _attr_native_max_value = float(IDLE_TIMEOUT_SECONDS_MAX)
    _attr_native_step = 1
    _attr_mode = NumberMode.BOX
    _attr_native_unit_of_measurement = "s"

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        self._entry_id = entry_id
        self._topic_prefix = topic_prefix.strip("/")
        self._mac = mac
        self._attr_unique_id = f"{mac}_idle_timeout_seconds"

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    @property
    def native_value(self) -> float | None:
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        if panel is None:
            return None
        return float(_coerce_uint_seconds(panel.parameters.get("idle_timeout_seconds")))

    async def async_set_native_value(self, value: float) -> None:
        sec = max(0, min(IDLE_TIMEOUT_SECONDS_MAX, int(value)))
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        screen = _idle_screen_from_panel(panel)
        payload = json.dumps({"screen": screen, "seconds": sec})
        topic = f"{self._topic_prefix}/device/{self._mac}/cmd/set_idle_timeout"
        await mqtt.async_publish(self.hass, topic, payload=payload, qos=1, retain=False)
        if panel is not None:
            panel.parameters["idle_timeout_screen"] = screen
            panel.parameters["idle_timeout_seconds"] = sec
        self.async_write_ha_state()
