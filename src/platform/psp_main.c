/*
 * psp_main.c - PSP entry point
 * Redneck Rampage PSP Port
 * Creates debug.log in same folder as EBOOT.PBP
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

PSP_MODULE_INFO("RedneckRampage", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(4096);

/* ============================================================
 * DEBUG LOG FILE - writes to memory stick
 * ============================================================ */
static FILE *g_log = NULL;

static void log_open(void) {
    /* Try multiple paths */
    g_log = fopen("ms0:/PSP/GAME/RedneckRampage/debug.log", "w");
    if (!g_log) g_log = fopen("debug.log", "w");
    if (!g_log) g_log = fopen("ms0:/debug.log", "w");
    if (g_log) {
        fprintf(g_log, "=== REDNECK RAMPAGE PSP DEBUG LOG ===\n");
        fflush(g_log);
    }
}

static void LOG(const char *fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);  /* Flush every line so we don't lose data on crash */
}

static void log_close(void) {
    if (g_log) {
        fprintf(g_log, "=== LOG END ===\n");
        fclose(g_log);
        g_log = NULL;
    }
}

/* ============================================================
 * Exit callback
 * ============================================================ */
static int exit_cb(int arg1, int arg2, void *common) {
    (void)arg1; (void)arg2; (void)common;
    LOG("EXIT CALLBACK triggered\n");
    log_close();
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

/* ============================================================
 * Find GRP file
 * ============================================================ */
static const char *find_grp(void) {
    static const char *paths[] = {
        "ms0:/PSP/GAME/RedneckRampage/REDNECK.GRP",
        "REDNECK.GRP",
        "./REDNECK.GRP",
        "ms0:/REDNECK.GRP",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        LOG("  Looking for: %s ... ", paths[i]);
        FILE *f = fopen(paths[i], "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fclose(f);
            LOG("FOUND (%ld bytes)\n", sz);
            return paths[i];
        }
        LOG("not found\n");
    }
    return NULL;
}

/* ============================================================
 * MAIN
 * ============================================================ */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* STEP 0: Open log file IMMEDIATELY */
    log_open();
    LOG("main() entered\n");
    LOG("sizeof(sectortype)=%d sizeof(walltype)=%d sizeof(spritetype)=%d\n",
        (int)sizeof(sectortype), (int)sizeof(walltype), (int)sizeof(spritetype));

    /* STEP 1: Callbacks */
    LOG("STEP 1: Setting up callbacks\n");
    setup_callbacks();
    LOG("  OK\n");

    /* STEP 2: CPU */
    LOG("STEP 2: CPU 333MHz\n");
    scePowerSetClockFrequency(333, 333, 166);
    LOG("  OK\n");

    /* STEP 3: Memory */
    LOG("STEP 3: Memory info\n");
    LOG("  Total free: %u KB\n", (unsigned)(sceKernelTotalFreeMemSize() / 1024));
    LOG("  Max block:  %u KB\n", (unsigned)(sceKernelMaxFreeMemSize() / 1024));

    /* STEP 4: Input */
    LOG("STEP 4: Input\n");
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    LOG("  OK\n");

    /* STEP 5: Find GRP */
    LOG("STEP 5: Searching for REDNECK.GRP\n");
    const char *grp = find_grp();
    if (!grp) {
        LOG("FATAL: REDNECK.GRP not found anywhere!\n");
        log_close();
        /* Show error on screen */
        pspDebugScreenInit();
        pspDebugScreenPrintf("REDNECK.GRP not found!\n");
        pspDebugScreenPrintf("Place it in ms0:/PSP/GAME/RedneckRampage/\n");
        pspDebugScreenPrintf("Check debug.log for details. Press HOME.\n");
        while(1) sceDisplayWaitVblankStart();
        return 1;
    }
    LOG("  Using GRP: %s\n", grp);

    /* STEP 6: Game init */
    LOG("STEP 6: game_init()\n");
    int ret = game_init(grp);
    LOG("  game_init returned: %d\n", ret);
    LOG("  Mem free after init: %u KB\n", (unsigned)(sceKernelTotalFreeMemSize() / 1024));
    if (ret != 0) {
        LOG("FATAL: game_init failed with code %d\n", ret);
        log_close();
        pspDebugScreenInit();
        pspDebugScreenPrintf("game_init failed: %d\nCheck debug.log\n", ret);
        while(1) sceDisplayWaitVblankStart();
        return 1;
    }

    /* STEP 7: Game loop */
    LOG("STEP 7: Starting game_run()\n");
    log_close();  /* Close log before game loop to free handle */

    game_run();

    game_shutdown();
    sceKernelExitGame();
    return 0;
}
