#pragma once

/** Init ES8311 speaker codec via BSP (idempotent). */
void audio_demo_init(void);

/** Start repeating 440 Hz beep while Penny's Room (or caller) is active. Non-blocking. */
void audio_demo_start(void);

/** Stop the beep task and close the codec stream. */
void audio_demo_stop(void);
