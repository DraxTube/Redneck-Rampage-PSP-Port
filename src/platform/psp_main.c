/*
 * psp_main.c - PSP entry point and system callbacks
 * Redneck Rampage PSP Port
 *
 * Sets up PSP module info, exit callbacks, CPU clock,
 * and launches the game. Includes debug screen logging.
 */

#include <pspkernel.h>
#include <pspdebug.h>
#include <psppower.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "game.h"

/* PSP Module Information */
PSP_MODULE_INFO("RedneckRampage", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(16384);  /* 16MB heap - safe value */

/* Debug logging macro */
#define DBG(...) do { \
    pspDebugScreenPrintf(__VA_ARGS__); \
    sceDisplayWaitVblankStart(); \
} while(0)

/* Log file on memory stick */
static FILE *g_logfile = NULL;

static void log_init(void) {
    g_logfile = fopen("ms0:/PSP/GAME/RedneckRampage/debug.log", "w");
    if (!g_logfile) {
        g_logfile = fopen("debug.log", "w");
    }
}

static void log_write(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* Write to debug screen */
    pspDebugScreenPrintf("%s", buf);

    /* Write to log file */
    if (g_logfile) {
        fputs(buf, g_logfile);
        fflush(g_logfile);
    }
}

static void log_close(void) {
    if (g_logfile) {
        fclose(g_logfile);
        g_logfile = NULL;
    }
}

/* ============================================================
 * Exit callback - handles HOME button
 * ============================================================ */
static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    log_close();
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
    "ms0:/PSP/GAME/RedneckRampage/REDNECK.GRP",
    "REDNECK.GRP",
    "./REDNECK.GRP",
    "ms0:/REDNECK.GRP",
    "host0:/REDNECK.GRP",
    NULL
};

static const char *find_grp(void) {
    for (int i = 0; grp_search_paths[i] != NULL; i++) {
        log_write("  Trying: %s ... ", grp_search_paths[i]);
        FILE *f = fopen(grp_search_paths[i], "rb");
        if (f) {
            /* Get file size for diagnostic */
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fclose(f);
            log_write("FOUND (%ld bytes)\n", size);
            return grp_search_paths[i];
        }
        log_write("not found\n");
    }
    return NULL;
}

/* ============================================================
 * Wait for button press (debug)
 * ============================================================ */
static void wait_button(const char *msg) {
    if (msg) {
        log_write("\n%s\n", msg);
    }
    log_write("Press X to continue...\n");
    sceDisplayWaitVblankStart();

    SceCtrlData pad;
    /* Wait for release first */
    do {
        sceCtrlPeekBufferPositive(&pad, 1);
    } while (pad.Buttons & PSP_CTRL_CROSS);
    /* Wait for press */
    do {
        sceCtrlPeekBufferPositive(&pad, 1);
        sceDisplayWaitVblankStart();
    } while (!(pad.Buttons & PSP_CTRL_CROSS));
}

/* ============================================================
 * Error screen with log info
 * ============================================================ */
static void show_error(const char *msg) {
    pspDebugScreenSetTextColor(0x000000FF); /* Red */
    log_write("\n=== ERROR ===\n");
    log_write("  %s\n\n", msg);
    pspDebugScreenSetTextColor(0x00FFFFFF); /* White */
    log_write("  Place REDNECK.GRP in:\n");
    log_write("  ms0:/PSP/GAME/RedneckRampage/REDNECK.GRP\n\n");
    log_write("  Check debug.log for details.\n");
    log_write("  Press HOME to exit.\n");
    log_close();

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

    /* Init debug screen FIRST */
    pspDebugScreenInit();
    pspDebugScreenSetBackColor(0x00000000);
    pspDebugScreenClear();
    pspDebugScreenSetTextColor(0x0000FFFF); /* Yellow */

    log_write("========================================\n");
    log_write("  REDNECK RAMPAGE PSP - DEBUG LOG\n");
    log_write("========================================\n\n");

    /* Setup exit callback */
    log_write("[1/7] Setting up callbacks... ");
    setup_callbacks();
    log_write("OK\n");

    /* Init log file */
    log_write("[2/7] Opening log file... ");
    log_init();
    log_write("OK\n");

    /* Set CPU clock */
    log_write("[3/7] Setting CPU to 333MHz... ");
    scePowerSetClockFrequency(333, 333, 166);
    log_write("OK\n");

    /* Report memory */
    log_write("[4/7] Memory info:\n");
    log_write("  Free mem: %u KB\n", (unsigned)(sceKernelTotalFreeMemSize() / 1024));
    log_write("  Max block: %u KB\n", (unsigned)(sceKernelMaxFreeMemSize() / 1024));

    /* Find GRP file */
    log_write("[5/7] Searching for REDNECK.GRP...\n");
    const char *grp_path = find_grp();
    if (!grp_path) {
        show_error("REDNECK.GRP not found!");
        sceKernelExitGame();
        return 1;
    }
    log_write("  Using: %s\n", grp_path);

    /* Initialize game */
    log_write("[6/7] Initializing game engine...\n");
    pspDebugScreenSetTextColor(0x0000FF00); /* Green */
    int init_result = game_init(grp_path);
    if (init_result != 0) {
        pspDebugScreenSetTextColor(0x000000FF); /* Red */
        char errmsg[128];
        snprintf(errmsg, sizeof(errmsg), "game_init() failed with code %d", init_result);
        show_error(errmsg);
        sceKernelExitGame();
        return 1;
    }
    log_write("  Game init OK!\n");

    /* Start game */
    pspDebugScreenSetTextColor(0x0000FFFF); /* Yellow */
    log_write("[7/7] Starting game loop...\n");
    log_write("  Free mem now: %u KB\n\n", (unsigned)(sceKernelTotalFreeMemSize() / 1024));

    wait_button("All systems ready!");

    /* Run game loop */
    game_run();

    /* Cleanup */
    log_write("Game exited normally.\n");
    game_shutdown();
    log_close();

    sceKernelExitGame();
    return 0;
}
