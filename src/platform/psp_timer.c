/*
 * psp_timer.c - PSP high-resolution timer implementation
 * Redneck Rampage PSP Port
 */

#include "psp_timer.h"
#include <pspkernel.h>
#include <psprtc.h>

static uint64_t frame_start_time = 0;
static uint64_t last_frame_time = 0;
static uint32_t frame_delta_ms = 16;  /* Default ~60fps */
static float    current_fps = 30.0f;
static uint32_t tick_res = 0;

void psp_timer_init(void) {
    tick_res = sceRtcGetTickResolution(); /* Ticks per second */
    if (tick_res == 0) tick_res = 1000000;

    u64 tick;
    sceRtcGetCurrentTick(&tick);
    frame_start_time = (uint64_t)tick;
    last_frame_time = frame_start_time;
}

uint64_t psp_timer_get_us(void) {
    u64 tick;
    sceRtcGetCurrentTick(&tick);
    return (uint64_t)tick;
}

uint32_t psp_timer_get_ms(void) {
    return (uint32_t)(psp_timer_get_us() / 1000);
}

uint32_t psp_timer_frame_start(void) {
    uint64_t now = psp_timer_get_us();
    uint64_t delta = now - frame_start_time;
    frame_start_time = now;

    frame_delta_ms = (uint32_t)(delta / 1000);
    if (frame_delta_ms == 0) frame_delta_ms = 1;
    if (frame_delta_ms > 200) frame_delta_ms = 200; /* Cap at 5fps */

    /* Smooth FPS calculation */
    if (delta > 0) {
        float instant_fps = 1000000.0f / (float)delta;
        current_fps = current_fps * 0.9f + instant_fps * 0.1f;
    }

    return frame_delta_ms;
}

void psp_timer_frame_wait(int target_fps) {
    if (target_fps <= 0) return;

    uint64_t target_us = 1000000 / target_fps;
    uint64_t now = psp_timer_get_us();
    uint64_t elapsed = now - frame_start_time;

    if (elapsed < target_us) {
        uint64_t wait_us = target_us - elapsed;
        if (wait_us > 1000) {
            sceKernelDelayThread((SceUInt)wait_us);
        }
    }
}

float psp_timer_get_fps(void) {
    return current_fps;
}
