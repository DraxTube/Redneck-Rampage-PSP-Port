/*
 * weapons.h - Weapon definitions
 * Redneck Rampage PSP Port
 */

#ifndef WEAPONS_H
#define WEAPONS_H

#include "game.h"

/* Weapon stats */
typedef struct {
    const char *name;
    int32_t     damage;
    int32_t     fire_rate;       /* Ticks between shots */
    int32_t     ammo_per_shot;
    int         is_hitscan;      /* 1=instant, 0=projectile */
    int32_t     proj_speed;      /* Projectile speed (if not hitscan) */
    int32_t     spread;          /* Accuracy spread in angle units */
    int16_t     fire_sound;      /* Sound effect ID */
} weapon_stats_t;

extern const weapon_stats_t weapon_table[WPN_COUNT];

/* Fire the current weapon */
void weapon_fire(game_t *g);

/* Update weapon animation/state */
void weapon_update(game_t *g);

/* Draw weapon on screen (HUD overlay) */
void weapon_draw_hud(game_t *g);

#endif /* WEAPONS_H */
