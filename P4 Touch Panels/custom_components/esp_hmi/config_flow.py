"""Config flow for ESP HMI Panels."""

from __future__ import annotations

import voluptuous as vol

from homeassistant import config_entries

from .const import CONF_TOPIC_PREFIX, DEFAULT_TOPIC_PREFIX, DOMAIN


class EspHmiConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle a config flow for ESP HMI Panels."""

    VERSION = 1
    icon = "mdi:tablet-dashboard"

    async def async_step_user(self, user_input=None):
        if user_input is not None:
            return self.async_create_entry(title="ESP HMI Panels", data=user_input)

        schema = vol.Schema(
            {
                vol.Optional(CONF_TOPIC_PREFIX, default=DEFAULT_TOPIC_PREFIX): str,
            }
        )
        return self.async_show_form(step_id="user", data_schema=schema)

    async def async_step_import(self, user_input):
        """Import YAML config (advanced/legacy)."""
        # If already configured via UI, ignore import.
        if self._async_current_entries():
            return self.async_abort(reason="single_instance_allowed")
        return self.async_create_entry(title="ESP HMI Panels", data=user_input)

