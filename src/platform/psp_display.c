/*
 * psp_display.c - PSP display implementation
 * Redneck Rampage PSP Port
 *
 * Uses double-buffered VRAM framebuffers with nearest-neighbor
 * upscaling from 320x200 (Build engine) to 480x272 (PSP native).
 */

#include "psp_display.h"
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspge.h>
#include <string.h>

/* VRAM base (uncached) */
#define VRAM_BASE       ((void *)0x44000000)
#define VRAM_FB_SIZE    (PSP_BUF_W * PSP_SCR_H * 4)

/* Framebuffer pointers in VRAM */
static uint32_t *vram_fb0;
static uint32_t *vram_fb1;
static int current_fb = 0;

void psp_display_init(void) {
    /* Framebuffer 0 at VRAM offset 0 */
    vram_fb0 = (uint32_t *)VRAM_BASE;
    /* Framebuffer 1 right after */
    vram_fb1 = (uint32_t *)((uint8_t *)VRAM_BASE + VRAM_FB_SIZE);

    /* Clear both framebuffers */
    memset(vram_fb0, 0, VRAM_FB_SIZE);
    memset(vram_fb1, 0, VRAM_FB_SIZE);

    /* Set display mode */
    sceDisplaySetMode(0, PSP_SCR_W, PSP_SCR_H);

    /* Set initial framebuffer */
    sceDisplaySetFrameBuf(vram_fb0, PSP_BUF_W,
                          PSP_DISPLAY_PIXEL_FORMAT_8888, 1);

    current_fb = 0;
}

void psp_display_shutdown(void) {
    /* Nothing to free - VRAM is hardware */
}

uint32_t *psp_display_get_vram(void) {
    return (current_fb == 0) ? vram_fb1 : vram_fb0;
}

/*
 * Upscale 320x200 to 480x272 with nearest-neighbor sampling.
 * PSP VRAM stride is 512 pixels (not 480).
 */
void psp_display_blit(const uint32_t *src) {
    uint32_t *dst = psp_display_get_vram();

    /* Vertical centering: (272 - scaled_height) / 2 */
    /* Scale factors: 480/320 = 1.5x, 272/200 = 1.36x */
    /* We'll scale to fill: 480 wide, 272 tall */
    for (int y = 0; y < PSP_SCR_H; y++) {
        int src_y = y * YDIM / PSP_SCR_H;
        if (src_y >= YDIM) src_y = YDIM - 1;

        const uint32_t *src_row = &src[src_y * XDIM];
        uint32_t *dst_row = &dst[y * PSP_BUF_W];

        for (int x = 0; x < PSP_SCR_W; x++) {
            int src_x = x * XDIM / PSP_SCR_W;
            if (src_x >= XDIM) src_x = XDIM - 1;
            dst_row[x] = src_row[src_x];
        }
    }
}

void psp_display_flip(void) {
    /* Swap to the buffer we just drew into */
    uint32_t *show_buf = psp_display_get_vram();

    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf(show_buf, PSP_BUF_W,
                          PSP_DISPLAY_PIXEL_FORMAT_8888, 0);

    current_fb ^= 1;
}

void psp_display_clear(uint32_t color) {
    uint32_t *dst = psp_display_get_vram();
    for (int i = 0; i < PSP_BUF_W * PSP_SCR_H; i++) {
        dst[i] = color;
    }
}
