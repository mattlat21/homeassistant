"""Select platform for ESP HMI Panels."""

from __future__ import annotations

import json
import logging
from typing import Any

from homeassistant.components import mqtt
from homeassistant.components.select import SelectEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.dispatcher import async_dispatcher_connect
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import EspHmiRuntime, SIGNAL_NEW_DEVICE, SIGNAL_PARAMETERS_UPDATE
from .const import DATA_RUNTIME, DOMAIN, SCREEN_OPTIONS

_LOGGER = logging.getLogger(__name__)


def _coerce_uint_seconds(val: Any) -> int:
    if val is None:
        return 0
    if isinstance(val, bool):
        return int(val)
    if isinstance(val, (int, float)):
        return max(0, int(val))
    if isinstance(val, str):
        try:
            return max(0, int(float(val)))
        except ValueError:
            return 0
    return 0


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    runtime: EspHmiRuntime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]

    default_by_mac: dict[str, EspHmiDefaultScreenSelect] = {}
    idle_by_mac: dict[str, EspHmiIdleTimeoutScreenSelect] = {}

    @callback
    def _maybe_add_for_mac(mac: str) -> None:
        if mac in default_by_mac:
            return
        d = EspHmiDefaultScreenSelect(entry.entry_id, runtime.topic_prefix, mac)
        i = EspHmiIdleTimeoutScreenSelect(entry.entry_id, runtime.topic_prefix, mac)
        default_by_mac[mac] = d
        idle_by_mac[mac] = i
        async_add_entities([d, i])

    @callback
    def _on_new_device(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        _maybe_add_for_mac(mac)

    @callback
    def _on_parameters_update(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        for ent in (default_by_mac.get(mac), idle_by_mac.get(mac)):
            if ent is not None:
                ent.async_write_ha_state()

    for mac in list(runtime.panels):
        _maybe_add_for_mac(mac)

    entry.async_on_unload(async_dispatcher_connect(hass, SIGNAL_NEW_DEVICE, _on_new_device))
    entry.async_on_unload(
        async_dispatcher_connect(hass, SIGNAL_PARAMETERS_UPDATE, _on_parameters_update)
    )


class EspHmiDefaultScreenSelect(SelectEntity):
    """UI dropdown for the panel's default startup screen (persisted in NVS)."""

    _attr_has_entity_name = True
    _attr_name = "Default screen"
    _attr_options = list(SCREEN_OPTIONS)

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        self._entry_id = entry_id
        self._topic_prefix = topic_prefix.strip("/")
        self._mac = mac
        self._attr_unique_id = f"{mac}_default_screen_select"

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    @property
    def current_option(self) -> str | None:
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        if panel is None:
            return None
        val = panel.parameters.get("default_screen")
        if isinstance(val, str) and val in SCREEN_OPTIONS:
            return val
        return None

    async def async_select_option(self, option: str) -> None:
        if option not in SCREEN_OPTIONS:
            _LOGGER.debug("Ignoring unknown default screen option: %s", option)
            return
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        topic = f"{self._topic_prefix}/device/{self._mac}/cmd/set_default_screen"
        await mqtt.async_publish(self.hass, topic, payload=option, qos=1, retain=False)
        if panel is not None:
            panel.parameters["default_screen"] = option
        self.async_write_ha_state()


class EspHmiIdleTimeoutScreenSelect(SelectEntity):
    """Idle-return screen after timeout (NVS); published with current seconds via cmd/set_idle_timeout."""

    _attr_has_entity_name = True
    _attr_name = "Idle timeout screen"
    _attr_options = list(SCREEN_OPTIONS)

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        self._entry_id = entry_id
        self._topic_prefix = topic_prefix.strip("/")
        self._mac = mac
        self._attr_unique_id = f"{mac}_idle_timeout_screen_select"

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    @property
    def current_option(self) -> str | None:
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        if panel is None:
            return None
        val = panel.parameters.get("idle_timeout_screen")
        if isinstance(val, str) and val in SCREEN_OPTIONS:
            return val
        return None

    async def async_select_option(self, option: str) -> None:
        if option not in SCREEN_OPTIONS:
            _LOGGER.debug("Ignoring unknown idle timeout screen option: %s", option)
            return
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        sec = _coerce_uint_seconds(
            panel.parameters.get("idle_timeout_seconds") if panel is not None else 0
        )
        payload = json.dumps({"screen": option, "seconds": sec})
        topic = f"{self._topic_prefix}/device/{self._mac}/cmd/set_idle_timeout"
        await mqtt.async_publish(self.hass, topic, payload=payload, qos=1, retain=False)
        if panel is not None:
            panel.parameters["idle_timeout_screen"] = option
            panel.parameters["idle_timeout_seconds"] = sec
        self.async_write_ha_state()
