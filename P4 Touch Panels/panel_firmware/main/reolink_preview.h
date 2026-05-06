#pragma once

#include "lvgl.h"

/** LVGL canvas size (multiples of 8). JPEG from the camera may differ; it is scaled to this size. */
#define REOLINK_PREVIEW_W 720 // 464
#define REOLINK_PREVIEW_H 480 // 320

/**
 * Start background workers and timers after the Front Gate UI exists.
 * @param front_gate_screen  Root screen (for SCREEN_LOADED / UNLOADED).
 * @param canvas             LVGL canvas using RGB888 buffer REOLINK_PREVIEW_W x REOLINK_PREVIEW_H.
 */
void reolink_preview_bind(lv_obj_t *front_gate_screen, lv_obj_t *canvas);
