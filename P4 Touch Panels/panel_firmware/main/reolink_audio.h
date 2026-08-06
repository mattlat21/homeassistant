#pragma once

#include <stdbool.h>

/** Listen-only Reolink RTSP AAC → panel speaker (Penny Room). */

typedef struct {
    const char *host;
    const char *user;
    const char *password;
} reolink_audio_config_t;

void reolink_audio_init(void);

/** Start streaming camera audio (non-blocking). Copies string pointers from cfg. */
void reolink_audio_start(const reolink_audio_config_t *cfg);

void reolink_audio_stop(void);
