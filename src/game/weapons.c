/*
 * weapons.c - Weapon logic implementation
 * Redneck Rampage PSP Port
 */

#include "weapons.h"
#include "actors.h"
#include "player.h"
#include "psp_audio.h"

/* Weapon stats table */
const weapon_stats_t weapon_table[WPN_COUNT] = {
    /* name          dmg  rate ammo hitscan speed spread snd */
    { "Crowbar",      15,  12,  0,   1,    0,    0,     SND_CROWBAR_SWING },
    { "Revolver",     20,  15,  1,   1,    0,    16,    SND_REVOLVER_FIRE },
    { "Shotgun",      80,  26,  1,   1,    0,    64,    SND_SHOTGUN_FIRE },
    { "Rifle",        60,  20,  1,   1,    0,    4,     SND_RIFLE_FIRE },
    { "Dynamite",    100,  30,  1,   0,    512,  0,     SND_DYNAMITE_THROW },
    { "Crossbow",    120,  35,  1,   0,    768,  0,     SND_DYNAMITE_THROW },
    { "Powder Keg",  200,  52,  1,   0,    256,  0,     SND_DYNAMITE_THROW },
    { "Tit Gun",      25,   5,  1,   1,    0,    32,    SND_REVOLVER_FIRE },
    { "Bowling Ball", 80,  40,  1,   0,    384,  0,     SND_DYNAMITE_THROW },
    { "Moonshine",    0,   20,  1,   0,    0,    0,     SND_PICKUP_HEALTH },
};

/* Fire the current weapon */
void weapon_fire(game_t *g) {
    int wpn = g->cur_weapon;
    const weapon_stats_t *ws = &weapon_table[wpn];

    /* Check ammo (crowbar has unlimited ammo) */
    if (wpn != WPN_CROWBAR) {
        if (g->ammo[wpn].current < ws->ammo_per_shot) return;
        g->ammo[wpn].current -= ws->ammo_per_shot;
    }

    /* Set weapon cooldown */
    g->weapon_timer = ws->fire_rate;
    g->weapon_frame = 1;

    /* Moonshine: drink for health */
    if (wpn == WPN_MOONSHINE) {
        player_give_health(g, 10);
        return;
    }

    if (ws->is_hitscan) {
        /* Hitscan weapon */
        int32_t cosang = sintable[(g->player_ang + 512) & BUILDANG_MASK];
        int32_t sinang = sintable[g->player_ang & BUILDANG_MASK];

        /* Apply spread */
        int16_t spread_ang = g->player_ang;
        if (ws->spread > 0) {
            spread_ang += (int16_t)((g->ticcount * 17 + g->player_x) % (ws->spread * 2) - ws->spread);
        }
        int32_t vx = sintable[(spread_ang + 512) & BUILDANG_MASK];
        int32_t vy = sintable[spread_ang & BUILDANG_MASK];

        /* Look angle to Z velocity */
        int32_t vz = (100 - g->player_horiz) * 256;

        hitdata_t hit;
        hitscan(g->player_x, g->player_y, g->player_z,
                g->player_sectnum, vx, vy, vz, &hit);

        /* Shotgun fires multiple pellets */
        int pellets = (wpn == WPN_SHOTGUN) ? 7 : 1;
        int32_t total_damage = ws->damage;
        if (wpn == WPN_SHOTGUN) total_damage = ws->damage / pellets;

        for (int p = 0; p < pellets; p++) {
            if (p > 0) {
                /* Additional pellets with more spread */
                int16_t pang = g->player_ang +
                    (int16_t)((g->ticcount * (p+3) + p * 137) % 128 - 64);
                int32_t pvx = sintable[(pang + 512) & BUILDANG_MASK];
                int32_t pvy = sintable[pang & BUILDANG_MASK];
                int32_t pvz = vz + (int32_t)((p * 97) % 64 - 32);
                hitscan(g->player_x, g->player_y, g->player_z,
                        g->player_sectnum, pvx, pvy, pvz, &hit);
            }

            /* Check if we hit an actor */
            if (hit.sprite >= 0) {
                for (int a = 0; a < num_actors; a++) {
                    if (actors[a].sprite_idx == hit.sprite) {
                        actor_damage(g, a, total_damage);
                        break;
                    }
                }
            }
        }

        /* Crowbar has limited range */
        if (wpn == WPN_CROWBAR) {
            int32_t hit_dx = hit.x - g->player_x;
            int32_t hit_dy = hit.y - g->player_y;
            int32_t hit_dist = ksqrt(mulscale16(hit_dx, hit_dx) + mulscale16(hit_dy, hit_dy));
            if (hit_dist > 1024) {
                /* Too far - miss */
            }
        }
    } else {
        /* Projectile weapon */
        int si = insertsprite(g->player_sectnum, 1);
        if (si >= 0) {
            sprite[si].x = g->player_x;
            sprite[si].y = g->player_y;
            sprite[si].z = g->player_z;
            sprite[si].ang = g->player_ang;
            sprite[si].xrepeat = 16;
            sprite[si].yrepeat = 16;
            sprite[si].picnum = 1 ; /* Generic projectile tile */
            sprite[si].cstat = SPRCSTAT_BLOCK;
            sprite[si].clipdist = 16;
            sprite[si].owner = 0; /* Player's projectile */

            /* Set velocity */
            int32_t cosang = sintable[(g->player_ang + 512) & BUILDANG_MASK];
            int32_t sinang = sintable[g->player_ang & BUILDANG_MASK];
            sprite[si].xvel = (int16_t)(mulscale16(ws->proj_speed, cosang) >> 8);
            sprite[si].yvel = (int16_t)(mulscale16(ws->proj_speed, sinang) >> 8);
            sprite[si].zvel = (int16_t)((100 - g->player_horiz) * 32);
            sprite[si].lotag = ws->damage;    /* Store damage in lotag */
            sprite[si].hitag = 120;           /* Lifetime in ticks */
            sprite[si].extra = wpn;           /* Weapon type */
        }
    }
}

void weapon_update(game_t *g) {
    /* Animate weapon frame */
    if (g->weapon_frame > 0) {
        g->weapon_frame++;
        if (g->weapon_frame > 4) {
            g->weapon_frame = 0;
        }
    }

    /* Update projectile sprites */
    for (int i = 0; i < numsprites; i++) {
        spritetype *spr = &sprite[i];
        if (spr->statnum == MAXSTATUS) continue;
        if (spr->owner < 0) continue; /* Not a projectile */
        if (spr->hitag <= 0) continue;

        /* Move projectile */
        int32_t ox = spr->x, oy = spr->y;
        spr->x += spr->xvel;
        spr->y += spr->yvel;
        spr->z += spr->zvel;
        spr->hitag--; /* Decrease lifetime */

        /* Update sector */
        int16_t newsect = spr->sectnum;
        updatesector(spr->x, spr->y, &newsect);

        if (newsect < 0 || spr->hitag <= 0) {
            /* Hit something or expired */
            /* Spawn explosion for explosive weapons */
            if (spr->extra == WPN_DYNAMITE || spr->extra == WPN_CROSSBOW ||
                spr->extra == WPN_POWDERKEG) {
                /* Damage nearby actors */
                for (int a = 0; a < num_actors; a++) {
                    int32_t dx = sprite[actors[a].sprite_idx].x - spr->x;
                    int32_t dy = sprite[actors[a].sprite_idx].y - spr->y;
                    int32_t dist = ksqrt(mulscale16(dx,dx) + mulscale16(dy,dy));
                    int32_t blast_radius = 2048;
                    if (dist < blast_radius) {
                        int32_t dmg = spr->lotag * (blast_radius - dist) / blast_radius;
                        actor_damage(g, a, dmg);
                    }
                }
                /* Self-damage */
                int32_t pdx = g->player_x - spr->x;
                int32_t pdy = g->player_y - spr->y;
                int32_t pdist = ksqrt(mulscale16(pdx,pdx) + mulscale16(pdy,pdy));
                if (pdist < 2048) {
                    int32_t self_dmg = spr->lotag * (2048 - pdist) / 2048 / 3;
                    player_damage(g, self_dmg);
                }
            } else {
                /* Direct-hit check for non-explosive projectiles */
                for (int a = 0; a < num_actors; a++) {
                    int32_t dx = sprite[actors[a].sprite_idx].x - spr->x;
                    int32_t dy = sprite[actors[a].sprite_idx].y - spr->y;
                    int32_t dist = ksqrt(mulscale16(dx,dx) + mulscale16(dy,dy));
                    if (dist < 512) {
                        actor_damage(g, a, spr->lotag);
                    }
                }
            }

            deletesprite(i);
            continue;
        }

        spr->sectnum = newsect;

        /* Collision with actors */
        for (int a = 0; a < num_actors; a++) {
            if (actors[a].ai_state == AI_DEAD) continue;
            int32_t dx = sprite[actors[a].sprite_idx].x - spr->x;
            int32_t dy = sprite[actors[a].sprite_idx].y - spr->y;
            int32_t dist = ksqrt(mulscale16(dx,dx) + mulscale16(dy,dy));
            if (dist < 256) {
                actor_damage(g, a, spr->lotag);
                spr->hitag = 0; /* Trigger removal next frame */
                break;
            }
        }
    }
}

/* Draw weapon HUD overlay */
void weapon_draw_hud(game_t *g) {
    /* Draw a simple weapon indicator at bottom center of framebuffer */
    int wpn = g->cur_weapon;
    int fire_frame = g->weapon_frame;

    /* Weapon "bob" position */
    int32_t bob_x = HALFXDIM;
    int32_t bob_y = YDIM - 40;
    int32_t bob_w = 40;
    int32_t bob_h = 40;

    if (fire_frame > 0) {
        bob_y -= fire_frame * 4; /* Recoil animation */
    }

    /* Draw colored rectangle as weapon placeholder */
    uint32_t wpn_colors[WPN_COUNT] = {
        0xFF808080, /* Crowbar - gray */
        0xFF0000C0, /* Revolver - blue */
        0xFF00C000, /* Shotgun - green */
        0xFF00C0C0, /* Rifle - cyan */
        0xFF0000FF, /* Dynamite - red */
        0xFF00FF00, /* Crossbow - bright green */
        0xFF0080FF, /* Powder Keg - orange */
        0xFFFF00FF, /* Tit Gun - magenta */
        0xFF00FFFF, /* Bowling Ball - yellow */
        0xFFC08000, /* Moonshine - brown */
    };

    uint32_t color = (wpn >= 0 && wpn < WPN_COUNT) ? wpn_colors[wpn] : 0xFFFFFFFF;

    int x1 = kclamp(bob_x - bob_w/2, 0, XDIM - 1);
    int x2 = kclamp(bob_x + bob_w/2, 0, XDIM);
    int y1 = kclamp(bob_y - bob_h/2, 0, YDIM - 1);
    int y2 = kclamp(bob_y + bob_h/2, 0, YDIM);

    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            /* Simple weapon shape */
            int dx = x - bob_x;
            int dy = y - bob_y;
            if (klabs(dx) < bob_w/3 || klabs(dy) < bob_h/3) {
                framebuffer[y * XDIM + x] = color;
            }
        }
    }
}
