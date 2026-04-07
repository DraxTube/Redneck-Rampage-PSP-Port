/*
 * actors.c - Actor AI implementation
 * Redneck Rampage PSP Port
 *
 * Implements enemy AI with states: idle, patrol, chase, attack, pain, dead.
 * Enemies are identified by sprite tile numbers from the GRP data.
 */

#include "actors.h"
#include "player.h"
#include "psp_audio.h"

actor_data_t actors[MAX_ACTORS];
int num_actors = 0;

/* Redneck Rampage tile number ranges for enemy types */
/* These are approximate - actual values depend on REDNECK.GRP content */
#define TILE_BILLYRAY_START  1400
#define TILE_BILLYRAY_END    1450
#define TILE_HOUND_START     1680
#define TILE_HOUND_END       1720
#define TILE_VIXEN_START     1760
#define TILE_VIXEN_END       1810
#define TILE_PIG_START       2000
#define TILE_PIG_END         2050
#define TILE_MOSQUITO_START  2220
#define TILE_MOSQUITO_END    2260

static actor_type_t identify_actor(int16_t picnum) {
    if (picnum >= TILE_BILLYRAY_START && picnum <= TILE_BILLYRAY_END)
        return ACTOR_BILLYRAY;
    if (picnum >= TILE_HOUND_START && picnum <= TILE_HOUND_END)
        return ACTOR_HOUND;
    if (picnum >= TILE_VIXEN_START && picnum <= TILE_VIXEN_END)
        return ACTOR_VIXEN;
    if (picnum >= TILE_PIG_START && picnum <= TILE_PIG_END)
        return ACTOR_PIG;
    if (picnum >= TILE_MOSQUITO_START && picnum <= TILE_MOSQUITO_END)
        return ACTOR_MOSQUITO;
    return ACTOR_NONE;
}

static void init_actor_stats(actor_data_t *a) {
    switch (a->type) {
        case ACTOR_BILLYRAY:
            a->health = 50;
            a->move_speed = 1024;
            a->attack_damage = 10;
            a->attack_range = 1024;
            a->sight_range = 16384;
            break;
        case ACTOR_HOUND:
            a->health = 20;
            a->move_speed = 2048;
            a->attack_damage = 8;
            a->attack_range = 512;
            a->sight_range = 12288;
            break;
        case ACTOR_VIXEN:
            a->health = 40;
            a->move_speed = 1536;
            a->attack_damage = 15;
            a->attack_range = 2048;
            a->sight_range = 16384;
            break;
        case ACTOR_PIG:
            a->health = 80;
            a->move_speed = 768;
            a->attack_damage = 20;
            a->attack_range = 1536;
            a->sight_range = 16384;
            break;
        case ACTOR_MOSQUITO:
            a->health = 10;
            a->move_speed = 3072;
            a->attack_damage = 5;
            a->attack_range = 256;
            a->sight_range = 8192;
            break;
        default:
            a->health = 30;
            a->move_speed = 1024;
            a->attack_damage = 5;
            a->attack_range = 1024;
            a->sight_range = 12288;
            break;
    }
}

void actors_init(game_t *g) {
    num_actors = 0;
    memset(actors, 0, sizeof(actors));

    (void)g;

    /* Scan map sprites and identify actors */
    for (int i = 0; i < numsprites && num_actors < MAX_ACTORS; i++) {
        spritetype *spr = &sprite[i];
        actor_type_t type = identify_actor(spr->picnum);

        if (type != ACTOR_NONE) {
            actor_data_t *a = &actors[num_actors];
            a->type = type;
            a->sprite_idx = i;
            a->target = -1;
            a->ai_state = AI_IDLE;
            a->ai_timer = 0;
            a->anim_frame = 0;
            a->anim_timer = 0;
            a->patrol_ang = spr->ang;
            init_actor_stats(a);
            num_actors++;
        }
    }
}

int actor_is_enemy(actor_type_t type) {
    return (type >= ACTOR_BILLYRAY && type <= ACTOR_MOSQUITO);
}

static int32_t actor_distance_to_player(actor_data_t *a, game_t *g) {
    spritetype *spr = &sprite[a->sprite_idx];
    int32_t dx = spr->x - g->player_x;
    int32_t dy = spr->y - g->player_y;
    return ksqrt(mulscale16(dx, dx) + mulscale16(dy, dy)) << 4;
}

static int actor_can_see_player(actor_data_t *a, game_t *g) {
    spritetype *spr = &sprite[a->sprite_idx];
    hitdata_t hit;
    int32_t dx = g->player_x - spr->x;
    int32_t dy = g->player_y - spr->y;
    int32_t dz = g->player_z - spr->z;

    int32_t len = ksqrt(mulscale16(dx, dx) + mulscale16(dy, dy));
    if (len <= 0) return 1;

    /* Normalize direction */
    int32_t vx = divscale16(dx, len);
    int32_t vy = divscale16(dy, len);
    int32_t vz = divscale16(dz, len);

    hitscan(spr->x, spr->y, spr->z, spr->sectnum, vx, vy, vz, &hit);

    /* Check if we hit a wall before reaching the player */
    if (hit.wall >= 0) {
        int32_t hit_dx = hit.x - spr->x;
        int32_t hit_dy = hit.y - spr->y;
        int32_t hit_dist = ksqrt(mulscale16(hit_dx, hit_dx) + mulscale16(hit_dy, hit_dy));

        int32_t player_dist = actor_distance_to_player(a, g);
        return (hit_dist >= (player_dist >> 4));
    }

    return 1;
}

static void actor_move_toward(actor_data_t *a, int32_t tx, int32_t ty) {
    spritetype *spr = &sprite[a->sprite_idx];
    int32_t dx = tx - spr->x;
    int32_t dy = ty - spr->y;
    int16_t target_ang = (int16_t)getangle(dx, dy);

    /* Turn toward target */
    int16_t diff = (target_ang - spr->ang) & BUILDANG_MASK;
    if (diff > 1024) diff -= 2048;
    int16_t turn = kclamp(diff, -64, 64);
    spr->ang = (spr->ang + turn) & BUILDANG_MASK;

    /* Move forward */
    int32_t cosang = sintable[(spr->ang + 512) & BUILDANG_MASK];
    int32_t sinang = sintable[spr->ang & BUILDANG_MASK];
    int32_t xvect = mulscale16(a->move_speed, cosang);
    int32_t yvect = mulscale16(a->move_speed, sinang);

    int32_t newx = spr->x + (xvect >> 14);
    int32_t newy = spr->y + (yvect >> 14);

    /* Simple collision check */
    int16_t newsect = spr->sectnum;
    clipmove(&newx, &newy, &spr->z, &newsect, xvect, yvect, 128, 4 << 8, 4 << 8);

    spr->x = newx;
    spr->y = newy;
    if (newsect >= 0) {
        spr->sectnum = newsect;
    }
}

static void actor_update_one(actor_data_t *a, game_t *g) {
    if (a->ai_state == AI_DEAD) return;

    spritetype *spr = &sprite[a->sprite_idx];
    if (spr->statnum == MAXSTATUS) return;

    int32_t dist = actor_distance_to_player(a, g);

    switch (a->ai_state) {
        case AI_IDLE:
            /* Check if player is in sight range */
            if (dist < a->sight_range && actor_can_see_player(a, g)) {
                a->ai_state = AI_CHASE;
                a->ai_timer = 0;
                a->target = -1; /* Chase player */
            }
            /* Periodic patrol */
            a->ai_timer++;
            if (a->ai_timer > 60) {
                a->ai_state = AI_PATROL;
                a->ai_timer = 52 + (a->sprite_idx & 31);
            }
            break;

        case AI_PATROL:
            /* Move in patrol direction */
            {
                int32_t cosang = sintable[(a->patrol_ang + 512) & BUILDANG_MASK];
                int32_t sinang = sintable[a->patrol_ang & BUILDANG_MASK];
                int32_t tx = spr->x + (cosang >> 4);
                int32_t ty = spr->y + (sinang >> 4);
                actor_move_toward(a, tx, ty);
            }
            a->ai_timer--;
            if (a->ai_timer <= 0) {
                a->ai_state = AI_IDLE;
                a->patrol_ang = (a->patrol_ang + 512 + (a->sprite_idx & 511)) & BUILDANG_MASK;
            }
            /* Check for player */
            if (dist < a->sight_range && actor_can_see_player(a, g)) {
                a->ai_state = AI_CHASE;
                a->ai_timer = 0;
            }
            break;

        case AI_CHASE:
            /* Move toward player */
            actor_move_toward(a, g->player_x, g->player_y);

            /* Close enough to attack? */
            if (dist < a->attack_range) {
                a->ai_state = AI_ATTACK;
                a->ai_timer = 8;
            }
            /* Lost sight? */
            if (dist > a->sight_range * 2 || !actor_can_see_player(a, g)) {
                a->ai_state = AI_IDLE;
                a->ai_timer = 0;
            }
            break;

        case AI_ATTACK:
            /* Face player */
            {
                int32_t dx = g->player_x - spr->x;
                int32_t dy = g->player_y - spr->y;
                spr->ang = (int16_t)getangle(dx, dy);
            }

            a->ai_timer--;
            if (a->ai_timer <= 0) {
                /* Deal damage to player */
                player_damage(g, a->attack_damage);
                a->ai_timer = 26; /* ~1 second cooldown */

                /* Go back to chasing */
                if (dist > a->attack_range) {
                    a->ai_state = AI_CHASE;
                }
            }
            break;

        case AI_PAIN:
            a->ai_timer--;
            if (a->ai_timer <= 0) {
                a->ai_state = AI_CHASE;
            }
            break;

        case AI_DEAD:
            break;
    }

    /* Animation */
    a->anim_timer++;
    if (a->anim_timer >= 4) {
        a->anim_timer = 0;
        a->anim_frame++;
    }

    /* Apply gravity to actor */
    if (spr->sectnum >= 0 && spr->sectnum < numsectors) {
        int32_t floor_z = sector[spr->sectnum].floorz;
        int32_t spr_height = (int32_t)(tilesizy[spr->picnum] * spr->yrepeat) << 2;
        int32_t target_z = floor_z - spr_height;
        if (spr->z > target_z) spr->z = target_z;
        if (spr->z < target_z - 512) {
            spr->z += 128; /* Gravity */
        }
    }
}

void actors_update(game_t *g) {
    for (int i = 0; i < num_actors; i++) {
        actor_update_one(&actors[i], g);
    }
}

int actor_spawn(actor_type_t type, int32_t x, int32_t y, int32_t z,
                int16_t ang, int16_t sectnum) {
    if (num_actors >= MAX_ACTORS) return -1;

    int si = insertsprite(sectnum, 1);
    if (si < 0) return -1;

    sprite[si].x = x;
    sprite[si].y = y;
    sprite[si].z = z;
    sprite[si].ang = ang;
    sprite[si].xrepeat = 40;
    sprite[si].yrepeat = 40;
    sprite[si].clipdist = 32;
    sprite[si].picnum = TILE_BILLYRAY_START; /* Default */

    actor_data_t *a = &actors[num_actors];
    a->type = type;
    a->sprite_idx = si;
    a->target = -1;
    a->ai_state = AI_IDLE;
    a->ai_timer = 0;
    a->patrol_ang = ang;
    init_actor_stats(a);

    return num_actors++;
}

void actor_damage(game_t *g, int actor_idx, int32_t damage) {
    if (actor_idx < 0 || actor_idx >= num_actors) return;
    actor_data_t *a = &actors[actor_idx];
    if (a->ai_state == AI_DEAD) return;

    a->health -= damage;
    if (a->health <= 0) {
        a->health = 0;
        a->ai_state = AI_DEAD;
        g->kills++;

        /* Mark sprite as dead (change tile, make non-blocking) */
        spritetype *spr = &sprite[a->sprite_idx];
        spr->cstat &= ~SPRCSTAT_BLOCK;
    } else {
        a->ai_state = AI_PAIN;
        a->ai_timer = 8;
    }
}
