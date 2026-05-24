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

WAKE_DISPLAY_BUTTON = ButtonEntityDescription(
    key="wake_display",
    name="Full brightness",
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    runtime: EspHmiRuntime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]

    entities: dict[str, ButtonEntity] = {}

    def _entity_key(mac: str, suffix: str) -> str:
        return f"{mac}_{suffix}"

    @callback
    def _maybe_add_for_mac(mac: str) -> None:
        new_entities: list[ButtonEntity] = []
        for suffix, cls, description in (
            ("reboot", EspHmiRebootButton, REBOOT_BUTTON),
            ("wake_display", EspHmiWakeDisplayButton, WAKE_DISPLAY_BUTTON),
        ):
            key = _entity_key(mac, suffix)
            if key in entities:
                continue
            ent = cls(entry.entry_id, runtime.topic_prefix, mac, description)
            entities[key] = ent
            new_entities.append(ent)
        if new_entities:
            async_add_entities(new_entities)

    @callback
    def _on_new_device(entry_id: str, mac: str) -> None:
        if entry_id != entry.entry_id:
            return
        _maybe_add_for_mac(mac)

    for mac in list(runtime.panels):
        _maybe_add_for_mac(mac)

    entry.async_on_unload(async_dispatcher_connect(hass, SIGNAL_NEW_DEVICE, _on_new_device))


class _EspHmiPanelButton(ButtonEntity):
    _attr_has_entity_name = True

    def __init__(
        self,
        entry_id: str,
        topic_prefix: str,
        mac: str,
        description: ButtonEntityDescription,
    ) -> None:
        self.entity_description = description
        self._entry_id = entry_id
        self._topic_prefix = topic_prefix.strip("/")
        self._mac = mac
        self._attr_unique_id = f"{mac}_{description.key}"


class EspHmiRebootButton(_EspHmiPanelButton):
    """Reboot the panel via MQTT."""

    def __init__(
        self,
        entry_id: str,
        topic_prefix: str,
        mac: str,
        description: ButtonEntityDescription = REBOOT_BUTTON,
    ) -> None:
        super().__init__(entry_id, topic_prefix, mac, description)

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    async def async_press(self) -> None:
        topic = f"{self._topic_prefix}/device/{self._mac}/cmd/reboot"
        await mqtt.async_publish(self.hass, topic, payload="1", qos=1, retain=False)


class EspHmiWakeDisplayButton(_EspHmiPanelButton):
    """Restore normal brightness and reset display inactivity timers."""

    def __init__(
        self,
        entry_id: str,
        topic_prefix: str,
        mac: str,
        description: ButtonEntityDescription = WAKE_DISPLAY_BUTTON,
    ) -> None:
        super().__init__(entry_id, topic_prefix, mac, description)

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    async def async_press(self) -> None:
        topic = f"{self._topic_prefix}/device/{self._mac}/cmd/wake_display"
        await mqtt.async_publish(self.hass, topic, payload="1", qos=1, retain=False)
