/*
 * psp_main.c - PSP entry point (minimal/debug version)
 * Redneck Rampage PSP Port
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "game.h"

/* PSP Module Information */
PSP_MODULE_INFO("RedneckRampage", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(4096);  /* 4MB heap only - keep it small */

/* Use pspDebugScreenPrintf as log */
#define LOG(...) pspDebugScreenPrintf(__VA_ARGS__)

/* Exit callback */
static int running = 1;

static int exit_cb(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    running = 0;
    sceKernelExitGame();
    return 0;
}

static int cb_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    int cbid = sceKernelCreateCallback("exit_cb", exit_cb, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    int thid = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, NULL);
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

/* GRP search */
static const char *find_grp(void) {
    static const char *paths[] = {
        "ms0:/PSP/GAME/RedneckRampage/REDNECK.GRP",
        "REDNECK.GRP",
        "./REDNECK.GRP",
        "ms0:/REDNECK.GRP",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        LOG("  Try: %s ... ", paths[i]);
        FILE *f = fopen(paths[i], "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fclose(f);
            LOG("OK (%ld bytes)\n", sz);
            return paths[i];
        }
        LOG("no\n");
    }
    return NULL;
}

/* Wait for X button */
static void wait_x(void) {
    LOG("\n>> Press X to continue <<\n");
    SceCtrlData pad;
    do { sceCtrlPeekBufferPositive(&pad, 1); sceDisplayWaitVblankStart(); }
    while (!(pad.Buttons & PSP_CTRL_CROSS));
    do { sceCtrlPeekBufferPositive(&pad, 1); sceDisplayWaitVblankStart(); }
    while (pad.Buttons & PSP_CTRL_CROSS);
}

/* Main */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* STEP 0: Debug screen first thing */
    pspDebugScreenInit();
    pspDebugScreenSetBackColor(0x00000000);
    pspDebugScreenClear();

    pspDebugScreenSetTextColor(0x0000FFFF);
    LOG("=================================\n");
    LOG(" REDNECK RAMPAGE PSP DEBUG v2\n");
    LOG("=================================\n\n");

    /* STEP 1: Callbacks */
    LOG("[1] Callbacks... ");
    setup_callbacks();
    LOG("OK\n");

    /* STEP 2: CPU */
    LOG("[2] CPU 333MHz... ");
    scePowerSetClockFrequency(333, 333, 166);
    LOG("OK\n");

    /* STEP 3: Memory report */
    LOG("[3] Memory:\n");
    LOG("    Total free: %u KB\n", (unsigned)(sceKernelTotalFreeMemSize() / 1024));
    LOG("    Max block:  %u KB\n", (unsigned)(sceKernelMaxFreeMemSize() / 1024));

    /* STEP 4: Input */
    LOG("[4] Input init... ");
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    LOG("OK\n");

    /* STEP 5: Search GRP */
    LOG("[5] Searching GRP...\n");
    const char *grp = find_grp();
    if (!grp) {
        pspDebugScreenSetTextColor(0x000000FF);
        LOG("\n!! REDNECK.GRP NOT FOUND !!\n\n");
        pspDebugScreenSetTextColor(0x00FFFFFF);
        LOG("Place REDNECK.GRP in:\n");
        LOG("ms0:/PSP/GAME/RedneckRampage/\n\n");
        LOG("Press HOME to exit.\n");
        while(running) sceDisplayWaitVblankStart();
        sceKernelExitGame();
        return 1;
    }

    /* STEP 6: Init game */
    LOG("[6] Game init...\n");
    int ret = game_init(grp);
    if (ret != 0) {
        pspDebugScreenSetTextColor(0x000000FF);
        LOG("\n!! game_init FAILED: %d !!\n", ret);
        LOG("Mem free: %u KB\n", (unsigned)(sceKernelTotalFreeMemSize() / 1024));
        wait_x();
        sceKernelExitGame();
        return 1;
    }

    pspDebugScreenSetTextColor(0x0000FF00);
    LOG("[7] INIT OK! Mem: %u KB free\n",
        (unsigned)(sceKernelTotalFreeMemSize() / 1024));

    wait_x();

    /* Run game */
    game_run();

    /* Cleanup */
    game_shutdown();
    sceKernelExitGame();
    return 0;
}
