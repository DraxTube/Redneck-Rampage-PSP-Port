/*
 * sectors.h - Sector effects definitions
 * Redneck Rampage PSP Port
 */

#ifndef SECTORS_H
#define SECTORS_H

#include "game.h"

/* Sector tag types (lotag values for interactive sectors) */
#define SECT_DOOR_UPDOWN     1    /* Door that opens up/down */
#define SECT_DOOR_SLIDE      2    /* Sliding door */
#define SECT_ELEVATOR_UP     3    /* Elevator going up */
#define SECT_ELEVATOR_DOWN   4    /* Elevator going down */
#define SECT_LIGHT_BLINK     5    /* Blinking light */
#define SECT_LIGHT_STROBE    6    /* Strobe light */
#define SECT_DAMAGE_LOW      7    /* Low damage floor */
#define SECT_DAMAGE_HIGH     8    /* High damage floor */
#define SECT_TELEPORT        9    /* Teleporter */
#define SECT_SECRET          10   /* Secret area */
#define SECT_WATER           11   /* Water sector */
#define SECT_END_LEVEL       12   /* End of level trigger */

/* Animated sector state */
typedef struct {
    int16_t sectnum;
    int16_t type;           /* SECT_* type */
    int32_t target_z;       /* Target floor/ceiling Z */
    int32_t original_z;     /* Original floor/ceiling Z */
    int32_t speed;          /* Movement speed */
    int     active;         /* Currently animating */
    int     direction;      /* 1=opening, -1=closing */
    int32_t timer;
} sector_anim_t;

#define MAX_SECTOR_ANIMS 128

extern sector_anim_t sector_anims[MAX_SECTOR_ANIMS];
extern int num_sector_anims;

/* Initialize sector effects from map data */
void sectors_init(game_t *g);

/* Update sector animations (call each tick) */
void sectors_update(game_t *g);

/* Activate a sector/wall action (player use) */
void sector_activate(game_t *g, int16_t wall_idx);

/* Check if player is in a special sector */
void sectors_check_player(game_t *g);

#endif /* SECTORS_H */
