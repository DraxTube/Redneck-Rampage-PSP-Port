/*
 * player.c - Player logic implementation
 * Redneck Rampage PSP Port
 */

#include "player.h"
#include "weapons.h"
#include "psp_audio.h"

void player_init(game_t *g, int32_t x, int32_t y, int32_t z,
                 int16_t ang, int16_t sectnum) {
    g->player_x = x;
    g->player_y = y;
    g->player_z = z - PLAYER_EYE_HEIGHT;
    g->player_ang = ang;
    g->player_horiz = 100;
    g->player_sectnum = sectnum;

    g->health = PLAYER_START_HEALTH;
    g->armor = 0;
    g->cur_weapon = WPN_CROWBAR;
    g->on_ground = 1;
    g->crouching = 0;
    g->jumping = 0;
    g->running = 0;
    g->vel_x = 0;
    g->vel_y = 0;
    g->vel_z = 0;
    g->weapon_timer = 0;
    g->weapon_frame = 0;
    g->firing = 0;

    /* Start with crowbar and revolver */
    memset(g->weapon_owned, 0, sizeof(g->weapon_owned));
    g->weapon_owned[WPN_CROWBAR] = 1;
    g->weapon_owned[WPN_REVOLVER] = 1;

    memset(g->ammo, 0, sizeof(g->ammo));
    g->ammo[WPN_CROWBAR].current = 1;   g->ammo[WPN_CROWBAR].max = 1;
    g->ammo[WPN_REVOLVER].current = 48; g->ammo[WPN_REVOLVER].max = 200;
    g->ammo[WPN_SHOTGUN].max = 50;
    g->ammo[WPN_RIFLE].max = 100;
    g->ammo[WPN_DYNAMITE].max = 50;
    g->ammo[WPN_CROSSBOW].max = 50;
    g->ammo[WPN_POWDERKEG].max = 10;
    g->ammo[WPN_TITGUN].max = 200;
    g->ammo[WPN_BOWLINGBALL].max = 20;
    g->ammo[WPN_MOONSHINE].max = 100;

    memset(g->keys, 0, sizeof(g->keys));
}

void player_update(game_t *g) {
    psp_input_t *inp = &g->input;
    int32_t move_speed = g->running ? PLAYER_RUN_SPEED : PLAYER_MOVE_SPEED;

    /* ---- Turning (analog stick horizontal or D-pad left/right) ---- */
    int32_t turn_amount = 0;
    if (inp->look_lr != 0) {
        turn_amount = inp->look_lr * PLAYER_TURN_SPEED / 128;
    }
    g->player_ang = (g->player_ang + turn_amount) & BUILDANG_MASK;

    /* ---- Look up/down (analog stick vertical) ---- */
    if (inp->look_ud != 0) {
        g->player_horiz += inp->look_ud / 8;
        g->player_horiz = (int16_t)kclamp(g->player_horiz, 0, 200);
    }

    /* ---- Forward/backward movement ---- */
    int32_t cosang = sintable[(g->player_ang + 512) & BUILDANG_MASK];
    int32_t sinang = sintable[g->player_ang & BUILDANG_MASK];

    int32_t xvect = 0, yvect = 0;

    if (inp->move_fb > 0) {
        /* Forward */
        xvect += mulscale16(move_speed, cosang);
        yvect += mulscale16(move_speed, sinang);
    } else if (inp->move_fb < 0) {
        /* Backward */
        xvect -= mulscale16(move_speed / 2, cosang);
        yvect -= mulscale16(move_speed / 2, sinang);
    }

    /* ---- Strafing (D-pad left/right) ---- */
    if (inp->move_lr != 0) {
        int32_t strafe = move_speed * inp->move_lr / 128;
        xvect += mulscale16(strafe, sinang);
        yvect -= mulscale16(strafe, cosang);
    }

    /* ---- Apply movement with collision ---- */
    if (xvect != 0 || yvect != 0) {
        clipmove(&g->player_x, &g->player_y, &g->player_z,
                 &g->player_sectnum, xvect, yvect, 128, 4 << 8, 4 << 8);
    }

    /* ---- Running ---- */
    g->running = inp->run;

    /* ---- Jumping ---- */
    if (inp->jump && g->on_ground && !g->jumping) {
        g->vel_z = PLAYER_JUMP_VEL;
        g->on_ground = 0;
        g->jumping = 1;
    }

    /* ---- Crouching ---- */
    g->crouching = inp->crouch;

    /* ---- Gravity & Z-movement ---- */
    int32_t ceilz, florz;
    getzrange(g->player_x, g->player_y, g->player_z,
              g->player_sectnum, &ceilz, &florz, 128);

    int32_t target_z = florz - PLAYER_EYE_HEIGHT;
    if (g->crouching) {
        target_z = florz - (PLAYER_EYE_HEIGHT / 2);
    }

    if (!g->on_ground) {
        g->vel_z += PLAYER_GRAVITY;
        g->player_z += g->vel_z;

        if (g->player_z >= target_z) {
            g->player_z = target_z;
            g->vel_z = 0;
            g->on_ground = 1;
            g->jumping = 0;
        }
        if (g->player_z <= ceilz + (4 << 8)) {
            g->player_z = ceilz + (4 << 8);
            g->vel_z = 0;
        }
    } else {
        g->player_z = target_z;
        g->vel_z = 0;
    }

    /* ---- Weapon switching ---- */
    if (inp->next_weapon) {
        int w = g->cur_weapon;
        for (int i = 0; i < WPN_COUNT; i++) {
            w = (w + 1) % WPN_COUNT;
            if (g->weapon_owned[w]) {
                g->cur_weapon = w;
                g->weapon_timer = 10;
                g->weapon_frame = 0;
                break;
            }
        }
    }
    if (inp->prev_weapon) {
        int w = g->cur_weapon;
        for (int i = 0; i < WPN_COUNT; i++) {
            w = (w - 1 + WPN_COUNT) % WPN_COUNT;
            if (g->weapon_owned[w]) {
                g->cur_weapon = w;
                g->weapon_timer = 10;
                g->weapon_frame = 0;
                break;
            }
        }
    }

    /* ---- Firing ---- */
    g->firing = inp->fire;
    if (g->firing && g->weapon_timer <= 0) {
        weapon_fire(g);
    }
    if (g->weapon_timer > 0) {
        g->weapon_timer--;
    }

    /* ---- Use / Interact ---- */
    if (inp->use) {
        /* Check for usable walls/sprites in front of player */
        hitdata_t hit;
        int32_t vx = mulscale16(1024, cosang);
        int32_t vy = mulscale16(1024, sinang);
        hitscan(g->player_x, g->player_y, g->player_z,
                g->player_sectnum, vx, vy, 0, &hit);

        /* Trigger sector/wall actions - handled by sectors.c */
        if (hit.wall >= 0) {
            extern void sector_activate(game_t *g, int16_t wall_idx);
            sector_activate(g, hit.wall);
        }
    }

    /* ---- Update sector ---- */
    updatesector(g->player_x, g->player_y, &g->player_sectnum);
}

void player_damage(game_t *g, int32_t amount) {
    /* Absorb damage with armor first */
    if (g->armor > 0) {
        int32_t armor_absorb = amount / 2;
        if (armor_absorb > g->armor) armor_absorb = g->armor;
        g->armor -= armor_absorb;
        amount -= armor_absorb;
    }

    g->health -= amount;
    if (g->health <= 0) {
        g->health = 0;
        g->state = GAMESTATE_GAMEOVER;
    }
}

void player_give_weapon(game_t *g, int weapon_id) {
    if (weapon_id >= 0 && weapon_id < WPN_COUNT) {
        g->weapon_owned[weapon_id] = 1;
    }
}

void player_give_ammo(game_t *g, int weapon_id, int amount) {
    if (weapon_id >= 0 && weapon_id < WPN_COUNT) {
        g->ammo[weapon_id].current += amount;
        if (g->ammo[weapon_id].current > g->ammo[weapon_id].max)
            g->ammo[weapon_id].current = g->ammo[weapon_id].max;
    }
}

void player_give_health(game_t *g, int amount) {
    g->health += amount;
    if (g->health > PLAYER_MAX_HEALTH)
        g->health = PLAYER_MAX_HEALTH;
}

void player_give_armor(game_t *g, int amount) {
    g->armor += amount;
    if (g->armor > PLAYER_MAX_ARMOR)
        g->armor = PLAYER_MAX_ARMOR;
}

void player_give_key(game_t *g, int key_id) {
    if (key_id >= 0 && key_id < 4)
        g->keys[key_id] = 1;
}

int player_has_key(game_t *g, int key_id) {
    if (key_id >= 0 && key_id < 4)
        return g->keys[key_id];
    return 0;
}
