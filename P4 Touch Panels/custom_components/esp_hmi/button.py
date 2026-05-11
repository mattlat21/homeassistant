"""Button platform for ESP HMI Panels."""

from __future__ import annotations

import logging

from homeassistant.components.button import ButtonEntity, ButtonEntityDescription
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.dispatcher import async_dispatcher_connect
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from homeassistant.components import mqtt

from . import EspHmiRuntime, SIGNAL_NEW_DEVICE
from .const import DATA_RUNTIME, DOMAIN

_LOGGER = logging.getLogger(__name__)


REBOOT_BUTTON = ButtonEntityDescription(
    key="reboot",
    name="Reboot",
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    runtime: EspHmiRuntime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]

    entities: dict[str, EspHmiRebootButton] = {}

    @callback
    def _maybe_add_for_mac(mac: str) -> None:
        if mac in entities:
            return
        ent = EspHmiRebootButton(entry.entry_id, runtime.topic_prefix, mac)
        entities[mac] = ent
        async_add_entities([ent])

    @callback
    def _on_new_device(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        _maybe_add_for_mac(mac)

    for mac in list(runtime.panels):
        _maybe_add_for_mac(mac)

    entry.async_on_unload(async_dispatcher_connect(hass, SIGNAL_NEW_DEVICE, _on_new_device))


class EspHmiRebootButton(ButtonEntity):
    """Reboot the panel via MQTT."""

    entity_description = REBOOT_BUTTON
    _attr_has_entity_name = True

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        self._entry_id = entry_id
        self._topic_prefix = topic_prefix.strip("/")
        self._mac = mac
        self._attr_unique_id = f"{mac}_reboot"

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    async def async_press(self) -> None:
        topic = f"{self._topic_prefix}/device/{self._mac}/cmd/reboot"
        await mqtt.async_publish(self.hass, topic, payload="1", qos=1, retain=False)

