"""Number platform for ESP HMI Panels (idle timeout and display power)."""

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
    DISPLAY_BRIGHTNESS_MAX,
    DISPLAY_BRIGHTNESS_MIN,
    DISPLAY_FADE_SECONDS_MAX,
    DISPLAY_TIMEOUT_SECONDS_MAX,
    DOMAIN,
    IDLE_TIMEOUT_SECONDS_MAX,
    SCREEN_OPTIONS,
)


def _coerce_uint_seconds(val: Any, max_sec: int) -> int:
    if val is None:
        return 0
    if isinstance(val, bool):
        return int(val)
    if isinstance(val, (int, float)):
        return max(0, min(max_sec, int(val)))
    if isinstance(val, str):
        try:
            return max(0, min(max_sec, int(float(val))))
        except ValueError:
            return 0
    return 0


def _coerce_brightness(val: Any) -> int:
    if val is None:
        return 0
    if isinstance(val, bool):
        return int(val)
    if isinstance(val, (int, float)):
        return max(DISPLAY_BRIGHTNESS_MIN, min(DISPLAY_BRIGHTNESS_MAX, int(val)))
    if isinstance(val, str):
        try:
            return max(
                DISPLAY_BRIGHTNESS_MIN,
                min(DISPLAY_BRIGHTNESS_MAX, int(float(val))),
            )
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


def _panel_param(panel: Any, key: str) -> Any | None:
    if panel is None:
        return None
    return panel.parameters.get(key)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    runtime: EspHmiRuntime = hass.data[DOMAIN][entry.entry_id][DATA_RUNTIME]

    entities: dict[str, NumberEntity] = {}

    def _entity_key(mac: str, suffix: str) -> str:
        return f"{mac}_{suffix}"

    @callback
    def _maybe_add_for_mac(mac: str) -> None:
        new_entities: list[NumberEntity] = []
        for suffix, cls in (
            ("idle_timeout_seconds", EspHmiIdleTimeoutSecondsNumber),
            ("normal_brightness", EspHmiNormalBrightnessNumber),
            ("dim_brightness", EspHmiDimBrightnessNumber),
            ("dim_timeout_seconds", EspHmiDimTimeoutSecondsNumber),
            ("screen_off_timeout_seconds", EspHmiScreenOffTimeoutSecondsNumber),
            ("brightness_fade_seconds", EspHmiBrightnessFadeSecondsNumber),
        ):
            key = _entity_key(mac, suffix)
            if key in entities:
                continue
            ent = cls(entry.entry_id, runtime.topic_prefix, mac)
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
        prefix = f"{mac}_"
        for key, ent in entities.items():
            if key.startswith(prefix):
                ent.async_write_ha_state()

    for mac in list(runtime.panels):
        _maybe_add_for_mac(mac)

    entry.async_on_unload(async_dispatcher_connect(hass, SIGNAL_NEW_DEVICE, _on_new_device))
    entry.async_on_unload(
        async_dispatcher_connect(hass, SIGNAL_PARAMETERS_UPDATE, _on_parameters_update)
    )


class _EspHmiPanelNumber(NumberEntity):
    _attr_has_entity_name = True
    _attr_mode = NumberMode.BOX

    def __init__(self, entry_id: str, topic_prefix: str, mac: str, unique_suffix: str) -> None:
        self._entry_id = entry_id
        self._topic_prefix = topic_prefix.strip("/")
        self._mac = mac
        self._attr_unique_id = f"{mac}_{unique_suffix}"

    @property
    def device_info(self):
        return {"identifiers": {(DOMAIN, self._mac)}}

    def _runtime_panel(self) -> tuple[EspHmiRuntime, Any | None]:
        runtime: EspHmiRuntime = self.hass.data[DOMAIN][self._entry_id][DATA_RUNTIME]
        return runtime, runtime.panels.get(self._mac)

    async def _publish_display_power(self, payload: dict[str, int]) -> None:
        topic = f"{self._topic_prefix}/device/{self._mac}/cmd/set_display_power"
        await mqtt.async_publish(
            self.hass,
            topic,
            payload=json.dumps(payload),
            qos=1,
            retain=False,
        )
        _, panel = self._runtime_panel()
        if panel is not None:
            panel.parameters.update(payload)


class EspHmiIdleTimeoutSecondsNumber(_EspHmiPanelNumber):
    """Seconds until idle return (0 = disabled); uses cmd/set_idle_timeout JSON with current screen slug."""

    _attr_name = "Idle timeout seconds"
    _attr_native_min_value = 0
    _attr_native_max_value = float(IDLE_TIMEOUT_SECONDS_MAX)
    _attr_native_step = 1
    _attr_native_unit_of_measurement = "s"

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        super().__init__(entry_id, topic_prefix, mac, "idle_timeout_seconds")

    @property
    def native_value(self) -> float | None:
        _, panel = self._runtime_panel()
        if panel is None:
            return None
        return float(
            _coerce_uint_seconds(_panel_param(panel, "idle_timeout_seconds"), IDLE_TIMEOUT_SECONDS_MAX)
        )

    async def async_set_native_value(self, value: float) -> None:
        sec = max(0, min(IDLE_TIMEOUT_SECONDS_MAX, int(value)))
        _, panel = self._runtime_panel()
        screen = _idle_screen_from_panel(panel)
        payload = json.dumps({"screen": screen, "seconds": sec})
        topic = f"{self._topic_prefix}/device/{self._mac}/cmd/set_idle_timeout"
        await mqtt.async_publish(self.hass, topic, payload=payload, qos=1, retain=False)
        if panel is not None:
            panel.parameters["idle_timeout_screen"] = screen
            panel.parameters["idle_timeout_seconds"] = sec
        self.async_write_ha_state()


class EspHmiNormalBrightnessNumber(_EspHmiPanelNumber):
    _attr_name = "Normal brightness"
    _attr_native_min_value = float(DISPLAY_BRIGHTNESS_MIN)
    _attr_native_max_value = float(DISPLAY_BRIGHTNESS_MAX)
    _attr_native_step = 1
    _attr_native_unit_of_measurement = "%"

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        super().__init__(entry_id, topic_prefix, mac, "normal_brightness")

    @property
    def native_value(self) -> float | None:
        _, panel = self._runtime_panel()
        if panel is None:
            return None
        return float(_coerce_brightness(_panel_param(panel, "normal_brightness")))

    async def async_set_native_value(self, value: float) -> None:
        pct = max(DISPLAY_BRIGHTNESS_MIN, min(DISPLAY_BRIGHTNESS_MAX, int(value)))
        await self._publish_display_power({"normal_brightness": pct})
        self.async_write_ha_state()


class EspHmiDimBrightnessNumber(_EspHmiPanelNumber):
    _attr_name = "Dim brightness"
    _attr_native_min_value = float(DISPLAY_BRIGHTNESS_MIN)
    _attr_native_max_value = float(DISPLAY_BRIGHTNESS_MAX)
    _attr_native_step = 1
    _attr_native_unit_of_measurement = "%"

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        super().__init__(entry_id, topic_prefix, mac, "dim_brightness")

    @property
    def native_value(self) -> float | None:
        _, panel = self._runtime_panel()
        if panel is None:
            return None
        return float(_coerce_brightness(_panel_param(panel, "dim_brightness")))

    async def async_set_native_value(self, value: float) -> None:
        pct = max(DISPLAY_BRIGHTNESS_MIN, min(DISPLAY_BRIGHTNESS_MAX, int(value)))
        await self._publish_display_power({"dim_brightness": pct})
        self.async_write_ha_state()


class EspHmiDimTimeoutSecondsNumber(_EspHmiPanelNumber):
    _attr_name = "Dim timeout seconds"
    _attr_native_min_value = 0
    _attr_native_max_value = float(DISPLAY_TIMEOUT_SECONDS_MAX)
    _attr_native_step = 1
    _attr_native_unit_of_measurement = "s"

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        super().__init__(entry_id, topic_prefix, mac, "dim_timeout_seconds")

    @property
    def native_value(self) -> float | None:
        _, panel = self._runtime_panel()
        if panel is None:
            return None
        return float(
            _coerce_uint_seconds(
                _panel_param(panel, "dim_timeout_seconds"),
                DISPLAY_TIMEOUT_SECONDS_MAX,
            )
        )

    async def async_set_native_value(self, value: float) -> None:
        sec = max(0, min(DISPLAY_TIMEOUT_SECONDS_MAX, int(value)))
        await self._publish_display_power({"dim_timeout_seconds": sec})
        self.async_write_ha_state()


class EspHmiScreenOffTimeoutSecondsNumber(_EspHmiPanelNumber):
    _attr_name = "Screen off timeout seconds"
    _attr_native_min_value = 0
    _attr_native_max_value = float(DISPLAY_TIMEOUT_SECONDS_MAX)
    _attr_native_step = 1
    _attr_native_unit_of_measurement = "s"

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        super().__init__(entry_id, topic_prefix, mac, "screen_off_timeout_seconds")

    @property
    def native_value(self) -> float | None:
        _, panel = self._runtime_panel()
        if panel is None:
            return None
        return float(
            _coerce_uint_seconds(
                _panel_param(panel, "screen_off_timeout_seconds"),
                DISPLAY_TIMEOUT_SECONDS_MAX,
            )
        )

    async def async_set_native_value(self, value: float) -> None:
        sec = max(0, min(DISPLAY_TIMEOUT_SECONDS_MAX, int(value)))
        await self._publish_display_power({"screen_off_timeout_seconds": sec})
        self.async_write_ha_state()


class EspHmiBrightnessFadeSecondsNumber(_EspHmiPanelNumber):
    _attr_name = "Brightness fade seconds"
    _attr_native_min_value = 0
    _attr_native_max_value = float(DISPLAY_FADE_SECONDS_MAX)
    _attr_native_step = 1
    _attr_native_unit_of_measurement = "s"

    def __init__(self, entry_id: str, topic_prefix: str, mac: str) -> None:
        super().__init__(entry_id, topic_prefix, mac, "brightness_fade_seconds")

    @property
    def native_value(self) -> float | None:
        _, panel = self._runtime_panel()
        if panel is None:
            return None
        return float(
            _coerce_uint_seconds(
                _panel_param(panel, "brightness_fade_seconds"),
                DISPLAY_FADE_SECONDS_MAX,
            )
        )

    async def async_set_native_value(self, value: float) -> None:
        sec = max(0, min(DISPLAY_FADE_SECONDS_MAX, int(value)))
        await self._publish_display_power({"brightness_fade_seconds": sec})
        self.async_write_ha_state()
