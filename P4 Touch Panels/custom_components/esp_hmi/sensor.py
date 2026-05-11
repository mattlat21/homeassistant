"""Sensor platform for ESP HMI Panels."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
import logging
from typing import Any

from homeassistant.components.sensor import SensorEntity, SensorEntityDescription
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.dispatcher import async_dispatcher_connect
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import EspHmiRuntime, SIGNAL_NEW_DEVICE, SIGNAL_PARAMETERS_UPDATE
from .const import DATA_RUNTIME, DOMAIN

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True, kw_only=True)
class EspHmiSensorDescription(SensorEntityDescription):
    """Describes an ESP HMI sensor."""

    value_key: str


SENSOR_DESCRIPTIONS: tuple[EspHmiSensorDescription, ...] = (
    EspHmiSensorDescription(
        key="firmware_version",
        name="Firmware version",
        value_key="firmware_version",
    ),
    EspHmiSensorDescription(
        key="wifi_ssid",
        name="Wi-Fi SSID",
        value_key="wifi_ssid",
    ),
    EspHmiSensorDescription(
        key="default_screen",
        name="Default screen",
        value_key="default_screen",
    ),
    EspHmiSensorDescription(
        key="chip_target",
        name="Chip target",
        value_key="chip_target",
    ),
    EspHmiSensorDescription(
        key="chip_revision",
        name="Chip revision",
        value_key="chip_revision",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    runtime: EspHmiRuntime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]

    entities: dict[tuple[str, str], EspHmiParameterSensor] = {}

    @callback
    def _maybe_add_for_mac(mac: str) -> None:
        panel = runtime.panels.get(mac)
        if panel is None:
            return
        new_entities: list[EspHmiParameterSensor] = []
        for desc in SENSOR_DESCRIPTIONS:
            key = (mac, desc.key)
            if key in entities:
                continue
            ent = EspHmiParameterSensor(entry.entry_id, mac, desc)
            entities[key] = ent
            new_entities.append(ent)
        if new_entities:
            async_add_entities(new_entities)

    @callback
    def _on_new_device(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        _maybe_add_for_mac(mac)

    @callback
    def _on_parameters_update(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        for desc in SENSOR_DESCRIPTIONS:
            ent = entities.get((mac, desc.key))
            if ent is not None:
                ent.async_write_ha_state()

    # Add entities for any panels already discovered.
    for mac in list(runtime.panels):
        _maybe_add_for_mac(mac)

    entry.async_on_unload(
        async_dispatcher_connect(hass, SIGNAL_NEW_DEVICE, _on_new_device)
    )
    entry.async_on_unload(
        async_dispatcher_connect(hass, SIGNAL_PARAMETERS_UPDATE, _on_parameters_update)
    )


class EspHmiParameterSensor(SensorEntity):
    """A sensor backed by a field in status/parameters."""

    _attr_has_entity_name = True

    def __init__(
        self,
        entry_id: str,
        mac: str,
        description: EspHmiSensorDescription,
    ) -> None:
        self.entity_description = description
        self._entry_id = entry_id
        self._mac = mac
        self._attr_unique_id = f"{mac}_{description.key}"

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    @property
    def native_value(self) -> Any:
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        if panel is None:
            return None
        return panel.parameters.get(self.entity_description.value_key)

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        if panel is None:
            return {}
        updated = panel.parameters_updated_at
        return {
            "mac": self._mac,
            "parameters_updated_at": updated.isoformat() if isinstance(updated, datetime) else None,
        }

