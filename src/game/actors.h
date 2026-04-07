/*
 * actors.h - Actor/enemy definitions
 * Redneck Rampage PSP Port
 */

#ifndef ACTORS_H
#define ACTORS_H

#include "game.h"

/* Actor instance data (stored in sprite.extra -> index) */
typedef struct {
    actor_type_t type;
    int16_t      sprite_idx;      /* Index in sprite[] */
    int32_t      health;
    int32_t      move_speed;
    int32_t      attack_damage;
    int32_t      attack_range;
    int32_t      sight_range;
    int16_t      target;          /* Target sprite index */

    /* AI state */
    int          ai_state;        /* 0=idle, 1=patrol, 2=chase, 3=attack, 4=pain, 5=dead */
    int32_t      ai_timer;        /* Ticks until next state change */
    int32_t      anim_frame;
    int32_t      anim_timer;
    int16_t      patrol_ang;      /* Direction for patrol */
} actor_data_t;

#define MAX_ACTORS 256

extern actor_data_t actors[MAX_ACTORS];
extern int num_actors;

/* AI states */
#define AI_IDLE    0
#define AI_PATROL  1
#define AI_CHASE   2
#define AI_ATTACK  3
#define AI_PAIN    4
#define AI_DEAD    5

/* Initialize actors from map sprites */
void actors_init(game_t *g);

/* Update all actors (one game tick) */
void actors_update(game_t *g);

/* Spawn a new actor */
int  actor_spawn(actor_type_t type, int32_t x, int32_t y, int32_t z,
                 int16_t ang, int16_t sectnum);

/* Damage an actor */
void actor_damage(game_t *g, int actor_idx, int32_t damage);

/* Check if an actor type is an enemy */
int  actor_is_enemy(actor_type_t type);

#endif /* ACTORS_H */
