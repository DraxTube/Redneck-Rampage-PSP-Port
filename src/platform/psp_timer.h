/*
 * psp_timer.h - PSP high-resolution timer
 * Redneck Rampage PSP Port
 */

#ifndef PSP_TIMER_H
#define PSP_TIMER_H

#include "compat.h"

/* Initialize timer system */
void psp_timer_init(void);

/* Get current time in microseconds */
uint64_t psp_timer_get_us(void);

/* Get current time in milliseconds */
uint32_t psp_timer_get_ms(void);

/* Frame timing: call at start of frame, returns delta time in ms */
uint32_t psp_timer_frame_start(void);

/* Frame timing: sleep until target frame time (for FPS cap) */
void psp_timer_frame_wait(int target_fps);

/* Get current FPS */
float psp_timer_get_fps(void);

#endif /* PSP_TIMER_H */
