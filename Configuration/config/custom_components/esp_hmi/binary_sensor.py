"""Binary sensor platform for ESP HMI Panels (MQTT session link state)."""

from __future__ import annotations

from homeassistant.components.binary_sensor import BinarySensorDeviceClass, BinarySensorEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.dispatcher import async_dispatcher_connect
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import EspHmiRuntime, SIGNAL_NEW_DEVICE, SIGNAL_STATUS_UPDATE
from .const import DATA_RUNTIME, DOMAIN


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    runtime: EspHmiRuntime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]

    entities: dict[str, EspHmiMqttConnectedBinarySensor] = {}

    @callback
    def _maybe_add_for_mac(mac: str) -> None:
        if mac in entities:
            return
        ent = EspHmiMqttConnectedBinarySensor(entry.entry_id, mac)
        entities[mac] = ent
        async_add_entities([ent])

    @callback
    def _on_new_device(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        _maybe_add_for_mac(mac)

    @callback
    def _on_status_update(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        ent = entities.get(mac)
        if ent is not None:
            ent.async_write_ha_state()

    for mac in list(runtime.panels):
        _maybe_add_for_mac(mac)

    entry.async_on_unload(async_dispatcher_connect(hass, SIGNAL_NEW_DEVICE, _on_new_device))
    entry.async_on_unload(async_dispatcher_connect(hass, SIGNAL_STATUS_UPDATE, _on_status_update))


class EspHmiMqttConnectedBinarySensor(BinarySensorEntity):
    """Mirrors retained `status/mqtt_connected` (`ON`/`OFF`; LWT publishes `OFF` when link drops uncleanly)."""

    _attr_has_entity_name = True
    _attr_name = "MQTT connected"
    _attr_device_class = BinarySensorDeviceClass.CONNECTIVITY

    def __init__(self, entry_id: str, mac: str) -> None:
        self._entry_id = entry_id
        self._mac = mac
        self._attr_unique_id = f"{mac}_mqtt_connected"

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    @property
    def is_on(self) -> bool | None:
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        panel = runtime.panels.get(self._mac)
        if panel is None:
            return None
        return panel.mqtt_connected
