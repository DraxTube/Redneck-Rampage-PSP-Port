/*
 * game.c - Main game loop and state management
 * Redneck Rampage PSP Port
 */

#include "game.h"
#include "player.h"
#include "actors.h"
#include "weapons.h"
#include "sectors.h"
#include "menus.h"
#include "grp.h"
#include "art.h"
#include "map.h"
#include "cache.h"
#include "psp_display.h"
#include "psp_input.h"
#include "psp_audio.h"
#include "psp_timer.h"

#include <pspdebug.h>
#include <pspkernel.h>

/* Debug log helper */
#define GLOG(...) pspDebugScreenPrintf(__VA_ARGS__)

game_t g_game;

/* Redneck Rampage map names: E{episode}L{level}.MAP */
static const char *get_map_name(int episode, int level) {
    static char mapname[16];
    snprintf(mapname, sizeof(mapname), "E%dL%d.MAP", episode, level);
    return mapname;
}

int game_init(const char *grp_path) {
    memset(&g_game, 0, sizeof(game_t));

    /* Initialize engine */
    GLOG("  [init] engine...\n");
    initengine();

    /* Open GRP file */
    GLOG("  [init] opening GRP: %s\n", grp_path);
    g_grp = grp_open(grp_path);
    if (!g_grp) {
        GLOG("  [FAIL] grp_open returned NULL\n");
        /* Try alternate paths */
        GLOG("  [init] trying alternate paths...\n");
        g_grp = grp_open("ms0:/PSP/GAME/RedneckRampage/REDNECK.GRP");
        if (!g_grp) {
            g_grp = grp_open("REDNECK.GRP");
            if (!g_grp) {
                GLOG("  [FAIL] All GRP paths failed!\n");
                return -1;
            }
        }
    }
    GLOG("  [OK] GRP opened, %d files\n", g_grp->num_files);
    GLOG("  Free mem: %u KB\n", (unsigned)(sceKernelTotalFreeMemSize() / 1024));

    /* Load palette */
    GLOG("  [init] loading palette...\n");
    if (loadpalette("PALETTE.DAT", "LOOKUP.DAT") != 0) {
        GLOG("  [FAIL] loadpalette failed\n");
        return -2;
    }
    GLOG("  [OK] palette loaded, %d shades\n", numshades);

    /* Load art tiles */
    GLOG("  [init] loading ART tiles...\n");
    int art_result = art_load_from_grp();
    if (art_result != 0) {
        GLOG("  [WARN] art_load returned %d (non-fatal)\n", art_result);
        /* Don't fail - some tiles may still have loaded */
    } else {
        GLOG("  [OK] ART tiles loaded\n");
    }
    GLOG("  Free mem: %u KB\n", (unsigned)(sceKernelTotalFreeMemSize() / 1024));

    /* Initialize subsystems */
    GLOG("  [init] display...\n");
    psp_display_init();
    GLOG("  [OK] display ready\n");

    GLOG("  [init] input...\n");
    psp_input_init();
    GLOG("  [OK] input ready\n");

    GLOG("  [init] audio...\n");
    psp_audio_init();
    GLOG("  [OK] audio ready\n");

    GLOG("  [init] timer...\n");
    psp_timer_init();
    GLOG("  [OK] timer ready\n");

    GLOG("  [init] menus...\n");
    menus_init();
    GLOG("  [OK] menus ready\n");

    /* Start at title screen */
    g_game.state = GAMESTATE_TITLE;
    g_game.episode = 1;
    g_game.level = 1;

    GLOG("  [init] COMPLETE! Free mem: %u KB\n",
         (unsigned)(sceKernelTotalFreeMemSize() / 1024));

    return 0;
}

void game_shutdown(void) {
    psp_audio_shutdown();
    psp_display_shutdown();
    uninitengine();

    if (g_grp) {
        grp_close(g_grp);
        g_grp = NULL;
    }
}

int game_load_level(int episode, int level) {
    const char *mapname = get_map_name(episode, level);

    /* Show loading screen */
    loading_draw(mapname);
    psp_display_blit(framebuffer);
    psp_display_flip();

    /* Load map */
    int32_t start_x, start_y, start_z;
    int16_t start_ang, start_sect;

    GLOG("Loading map: %s\n", mapname);

    if (map_load(mapname, &start_x, &start_y, &start_z,
                 &start_ang, &start_sect) != 0) {
        /* Try without the E prefix for alternate naming */
        char altname[16];
        snprintf(altname, sizeof(altname), "RR%d_%d.MAP", episode, level);
        GLOG("Trying alt: %s\n", altname);
        if (map_load(altname, &start_x, &start_y, &start_z,
                     &start_ang, &start_sect) != 0) {
            GLOG("Map load FAILED!\n");
            return -1;
        }
    }

    GLOG("Map loaded: %d sectors, %d walls, %d sprites\n",
         numsectors, numwalls, numsprites);

    /* Initialize player */
    player_init(&g_game, start_x, start_y, start_z, start_ang, start_sect);

    /* Initialize actors from map sprites */
    actors_init(&g_game);

    /* Initialize sector effects */
    sectors_init(&g_game);

    /* Count total enemies */
    g_game.total_kills = 0;
    for (int i = 0; i < num_actors; i++) {
        if (actor_is_enemy(actors[i].type)) {
            g_game.total_kills++;
        }
    }

    GLOG("Actors: %d, Enemies: %d\n", num_actors, g_game.total_kills);

    g_game.episode = episode;
    g_game.level = level;
    g_game.ticcount = 0;
    g_game.state = GAMESTATE_PLAYING;

    return 0;
}

void game_tick(void) {
    g_game.ticcount++;

    /* Process player */
    player_update(&g_game);

    /* Update actors */
    actors_update(&g_game);

    /* Update weapons (projectiles) */
    weapon_update(&g_game);

    /* Update sector animations */
    sectors_update(&g_game);

    /* Check player sector effects */
    sectors_check_player(&g_game);
}

void game_render(void) {
    /* Render the 3D view */
    drawrooms(g_game.player_x, g_game.player_y, g_game.player_z,
              g_game.player_ang, g_game.player_horiz, g_game.player_sectnum);

    /* Draw weapon overlay */
    weapon_draw_hud(&g_game);

    /* Draw HUD */
    hud_draw(&g_game);
}

void game_run(void) {
    int32_t tic_accumulator = 0;
    int32_t tic_interval_us = 1000000 / GAME_TICRATE;

    while (g_game.state != GAMESTATE_QUIT) {
        /* Frame timing */
        uint32_t delta_ms = psp_timer_frame_start();
        g_game.frametime_ms = delta_ms;
        tic_accumulator += delta_ms * 1000; /* Convert to microseconds */

        /* Poll input */
        psp_input_poll(&g_game.input);

        switch (g_game.state) {
            case GAMESTATE_TITLE:
                menus_update(&g_game);
                /* Clear and draw title */
                title_draw(&g_game);
                break;

            case GAMESTATE_MENU:
                menus_update(&g_game);
                /* Draw menu */
                if (g_game.state == GAMESTATE_MENU) {
                    for (int i = 0; i < XDIM * YDIM; i++) {
                        framebuffer[i] = 0xFF100808;
                    }
                    menus_draw(&g_game);
                }
                break;

            case GAMESTATE_LOADING:
                if (game_load_level(g_game.episode, g_game.level) != 0) {
                    g_game.state = GAMESTATE_MENU;
                }
                continue;

            case GAMESTATE_PLAYING:
                while (tic_accumulator >= tic_interval_us) {
                    game_tick();
                    tic_accumulator -= tic_interval_us;
                    if (tic_accumulator > tic_interval_us * 4) {
                        tic_accumulator = 0;
                    }
                }
                game_render();
                menus_update(&g_game);
                break;

            case GAMESTATE_PAUSED:
                game_render();
                menus_update(&g_game);
                menus_draw(&g_game);
                break;

            case GAMESTATE_GAMEOVER:
                game_render();
                menus_update(&g_game);
                menus_draw(&g_game);
                break;

            case GAMESTATE_VICTORY:
                game_render();
                menus_update(&g_game);
                menus_draw(&g_game);
                break;

            case GAMESTATE_QUIT:
                break;
        }

        /* Blit framebuffer to PSP display */
        psp_display_blit(framebuffer);
        psp_display_flip();

        /* Frame rate limit */
        psp_timer_frame_wait(GAME_TARGET_FPS);
    }
}
