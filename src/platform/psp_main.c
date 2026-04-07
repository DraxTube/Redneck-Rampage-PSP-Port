/*
 * psp_main.c - PSP entry point and system callbacks
 * Redneck Rampage PSP Port
 *
 * Sets up PSP module info, exit callbacks, CPU clock,
 * and launches the game.
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"

/* PSP Module Information */
PSP_MODULE_INFO("RedneckRampage", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);  /* Use most of available memory */

/* ============================================================
 * Exit callback - handles HOME button
 * ============================================================ */
static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("exit_cb", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static int setup_callbacks(void) {
    int thid = sceKernelCreateThread("cb_thread", callback_thread,
                                      0x11, 0xFA0, 0, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
    }
    return thid;
}

/* ============================================================
 * GRP file search paths
 * ============================================================ */
static const char *grp_search_paths[] = {
    "REDNECK.GRP",
    "./REDNECK.GRP",
    "ms0:/PSP/GAME/RedneckRampage/REDNECK.GRP",
    "ms0:/REDNECK.GRP",
    "host0:/REDNECK.GRP",
    NULL
};

static const char *find_grp(void) {
    for (int i = 0; grp_search_paths[i] != NULL; i++) {
        FILE *f = fopen(grp_search_paths[i], "rb");
        if (f) {
            fclose(f);
            return grp_search_paths[i];
        }
    }
    return NULL;
}

/* ============================================================
 * Error screen
 * ============================================================ */
static void show_error(const char *msg) {
    pspDebugScreenInit();
    pspDebugScreenSetBackColor(0x00200000);
    pspDebugScreenClear();
    pspDebugScreenSetTextColor(0x000000FF);
    pspDebugScreenPrintf("\n\n  REDNECK RAMPAGE PSP\n");
    pspDebugScreenSetTextColor(0x00FFFFFF);
    pspDebugScreenPrintf("  ====================\n\n");
    pspDebugScreenPrintf("  ERROR: %s\n\n", msg);
    pspDebugScreenPrintf("  Please ensure REDNECK.GRP is placed in:\n");
    pspDebugScreenPrintf("    ms0:/PSP/GAME/RedneckRampage/REDNECK.GRP\n\n");
    pspDebugScreenPrintf("  You need the original game data file.\n");
    pspDebugScreenPrintf("  (Buy the game from GOG.com or Steam)\n\n");
    pspDebugScreenPrintf("  Press HOME to exit.\n");

    /* Wait for exit */
    while (1) {
        sceDisplayWaitVblankStart();
    }
}

/* ============================================================
 * Main entry point
 * ============================================================ */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Setup exit callback */
    setup_callbacks();

    /* Set CPU clock to maximum for best performance */
    scePowerSetClockFrequency(333, 333, 166);

    /* Find GRP file */
    const char *grp_path = find_grp();
    if (!grp_path) {
        show_error("REDNECK.GRP not found!");
        sceKernelExitGame();
        return 1;
    }

    /* Initialize game */
    if (game_init(grp_path) != 0) {
        show_error("Failed to initialize game.\nCheck REDNECK.GRP is valid.");
        sceKernelExitGame();
        return 1;
    }

    /* Run game loop */
    game_run();

    /* Cleanup */
    game_shutdown();

    sceKernelExitGame();
    return 0;
}
