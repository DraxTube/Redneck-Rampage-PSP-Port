/*
 * player.h - Player definitions
 * Redneck Rampage PSP Port
 */

#ifndef PLAYER_H
#define PLAYER_H

#include "game.h"

/* Initialize player for a new level */
void player_init(game_t *g, int32_t x, int32_t y, int32_t z,
                 int16_t ang, int16_t sectnum);

/* Process player input and update movement */
void player_update(game_t *g);

/* Apply damage to player */
void player_damage(game_t *g, int32_t amount);

/* Give item to player */
void player_give_weapon(game_t *g, int weapon_id);
void player_give_ammo(game_t *g, int weapon_id, int amount);
void player_give_health(game_t *g, int amount);
void player_give_armor(game_t *g, int amount);
void player_give_key(game_t *g, int key_id);

/* Check if player has a specific key */
int  player_has_key(game_t *g, int key_id);

#endif /* PLAYER_H */
