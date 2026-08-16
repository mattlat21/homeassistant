#pragma once

#include "lvgl.h"

typedef void (*ui_confirm_dialog_cb_t)(void *user_data);

/**
 * Modal yes/no prompt on the top layer, used to gate actions that would otherwise fire immediately.
 * @a confirm_cb runs only when the user accepts; Cancel and the backdrop dismiss without invoking it.
 * @a title, @a message and @a confirm_text must outlive the dialog (string literals); @a message and
 * @a confirm_text may be NULL for a bare prompt with a default "Confirm" label.
 * Opening while a dialog is already up replaces it.
 */
void ui_confirm_dialog_open(const char *title, const char *message, const char *confirm_text,
                            ui_confirm_dialog_cb_t confirm_cb, void *user_data);

/** Close if open; safe to call unconditionally (e.g. on `LV_EVENT_SCREEN_UNLOADED`). */
void ui_confirm_dialog_close(void);
