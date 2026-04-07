/*
 * psp_display.h - PSP display/framebuffer API
 * Redneck Rampage PSP Port
 */

#ifndef PSP_DISPLAY_H
#define PSP_DISPLAY_H

#include "compat.h"

/* Initialize PSP display (double-buffered, 480x272, ABGR8888) */
void psp_display_init(void);

/* Shutdown display */
void psp_display_shutdown(void);

/* Get pointer to the current draw buffer (VRAM) */
uint32_t *psp_display_get_vram(void);

/* Copy the 320x200 framebuffer to the PSP screen with upscaling */
void psp_display_blit(const uint32_t *src_320x200);

/* Swap front/back buffers (vsync) */
void psp_display_flip(void);

/* Clear the back buffer */
void psp_display_clear(uint32_t color);

#endif /* PSP_DISPLAY_H */
