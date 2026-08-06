#pragma once

#include <stdbool.h>

#include "lvgl.h"

/** LVGL canvas size (multiples of 8). JPEG from the camera may differ; it is scaled to this size. */
#define REOLINK_PREVIEW_W 720 // 464
#define REOLINK_PREVIEW_H 480 // 320

/** Compile-time / bind-time Reolink Snap camera settings. */
typedef struct {
    const char *host;     /**< Empty or NULL disables this preview. */
    int port;
    bool use_https;
    const char *user;
    const char *password;
    int channel;
} reolink_cam_config_t;

/**
 * Start (or attach) a background snap pipeline for one camera after its UI exists.
 * Safe to call once per camera; shares one worker across all binds.
 * @param screen  Root screen (for SCREEN_LOADED / UNLOADED poll rate).
 * @param canvas  LVGL canvas using RGB888 buffer REOLINK_PREVIEW_W x REOLINK_PREVIEW_H
 *                (JPEG R,G,B is converted to LVGL B,G,R on blit).
 * @param cfg     Camera host/creds (copied by value; string pointers must remain valid).
 */
void reolink_preview_bind(lv_obj_t *screen, lv_obj_t *canvas, const reolink_cam_config_t *cfg);
