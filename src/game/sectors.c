/*
 * sectors.c - Sector effects implementation
 * Redneck Rampage PSP Port
 *
 * Handles interactive sectors: doors, elevators, damage floors,
 * teleporters, secrets, and level transitions.
 */

#include "sectors.h"
#include "player.h"

sector_anim_t sector_anims[MAX_SECTOR_ANIMS];
int num_sector_anims = 0;

void sectors_init(game_t *g) {
    num_sector_anims = 0;
    memset(sector_anims, 0, sizeof(sector_anims));

    /* Count secrets and killable enemies */
    g->total_secrets = 0;
    g->total_kills = 0;
    g->kills = 0;
    g->secrets = 0;

    /* Scan sectors for special types and pre-initialize animations */
    for (int i = 0; i < numsectors; i++) {
        int16_t tag = sector[i].lotag;

        if (tag == SECT_SECRET) {
            g->total_secrets++;
        }

        if ((tag == SECT_DOOR_UPDOWN || tag == SECT_DOOR_SLIDE ||
             tag == SECT_ELEVATOR_UP || tag == SECT_ELEVATOR_DOWN) &&
            num_sector_anims < MAX_SECTOR_ANIMS) {

            sector_anim_t *sa = &sector_anims[num_sector_anims];
            sa->sectnum = i;
            sa->type = tag;
            sa->active = 0;
            sa->direction = 0;
            sa->speed = 2048; /* Default speed */

            if (tag == SECT_DOOR_UPDOWN) {
                /* Door: ceiling moves up to open */
                sa->original_z = sector[i].ceilingz;
                /* Target: ceiling goes up to match the neighbor's ceiling */
                int w = sector[i].wallptr;
                int16_t ns = wall[w].nextsector;
                if (ns >= 0) {
                    sa->target_z = sector[ns].ceilingz;
                } else {
                    sa->target_z = sector[i].ceilingz - (64 << 8);
                }
            } else if (tag == SECT_ELEVATOR_UP || tag == SECT_ELEVATOR_DOWN) {
                sa->original_z = sector[i].floorz;
                int w = sector[i].wallptr;
                int16_t ns = wall[w].nextsector;
                if (ns >= 0) {
                    sa->target_z = (tag == SECT_ELEVATOR_UP) ?
                        sector[ns].ceilingz : sector[ns].floorz;
                } else {
                    int32_t offset = (tag == SECT_ELEVATOR_UP) ? -(64 << 8) : (64 << 8);
                    sa->target_z = sector[i].floorz + offset;
                }
            }

            num_sector_anims++;
        }
    }
}

static sector_anim_t *find_sector_anim(int16_t sectnum) {
    for (int i = 0; i < num_sector_anims; i++) {
        if (sector_anims[i].sectnum == sectnum) {
            return &sector_anims[i];
        }
    }
    return NULL;
}

void sector_activate(game_t *g, int16_t wall_idx) {
    if (wall_idx < 0 || wall_idx >= numwalls) return;

    walltype *wal = &wall[wall_idx];
    int16_t ns = wal->nextsector;

    /* Check wall tags first */
    if (wal->lotag != 0) {
        /* Switch wall - toggle associated sector */
        if (ns >= 0) {
            sector_anim_t *sa = find_sector_anim(ns);
            if (sa) {
                if (!sa->active) {
                    sa->active = 1;
                    sa->direction = 1; /* Open */
                } else {
                    sa->direction = -sa->direction; /* Toggle */
                }
            }
        }
        return;
    }

    /* Check if the wall's sector has a special tag */
    /* Find which sector this wall belongs to */
    for (int s = 0; s < numsectors; s++) {
        int sw = sector[s].wallptr;
        int ew = sw + sector[s].wallnum;
        if (wall_idx >= sw && wall_idx < ew) {
            /* This wall is in sector s */
            sector_anim_t *sa = find_sector_anim(s);
            if (sa && !sa->active) {
                sa->active = 1;
                sa->direction = 1;
            }

            /* Also check the sector on the other side */
            if (ns >= 0) {
                sa = find_sector_anim(ns);
                if (sa && !sa->active) {
                    sa->active = 1;
                    sa->direction = 1;
                }

                /* Check for locked doors (key required) */
                if (sector[ns].hitag >= 1 && sector[ns].hitag <= 4) {
                    int key_id = sector[ns].hitag - 1;
                    if (!player_has_key(g, key_id)) {
                        /* Need key - don't activate */
                        if (sa) sa->active = 0;
                        return;
                    }
                }
            }
            break;
        }
    }
}

void sectors_update(game_t *g) {
    (void)g;

    for (int i = 0; i < num_sector_anims; i++) {
        sector_anim_t *sa = &sector_anims[i];
        if (!sa->active) continue;

        sectortype *sec = &sector[sa->sectnum];

        switch (sa->type) {
            case SECT_DOOR_UPDOWN: {
                /* Animate ceiling */
                if (sa->direction > 0) {
                    /* Opening (ceiling goes up) */
                    sec->ceilingz -= sa->speed;
                    if (sec->ceilingz <= sa->target_z) {
                        sec->ceilingz = sa->target_z;
                        sa->direction = 0;
                        sa->timer = 130; /* Stay open ~5 seconds */
                    }
                } else if (sa->direction < 0) {
                    /* Closing */
                    sec->ceilingz += sa->speed;
                    if (sec->ceilingz >= sa->original_z) {
                        sec->ceilingz = sa->original_z;
                        sa->active = 0;
                        sa->direction = 0;
                    }
                } else {
                    /* Waiting to auto-close */
                    sa->timer--;
                    if (sa->timer <= 0) {
                        sa->direction = -1;
                    }
                }
                break;
            }

            case SECT_ELEVATOR_UP:
            case SECT_ELEVATOR_DOWN: {
                if (sa->direction > 0) {
                    int32_t target = sa->target_z;
                    int32_t diff = target - sec->floorz;
                    if (klabs(diff) <= sa->speed) {
                        sec->floorz = target;
                        sa->direction = 0;
                        sa->timer = 78; /* Wait ~3 seconds */
                    } else {
                        sec->floorz += (diff > 0) ? sa->speed : -sa->speed;
                    }
                    /* Move ceiling proportionally */
                    int32_t ceil_diff = sec->ceilingz - sec->floorz;
                    (void)ceil_diff;
                } else if (sa->direction < 0) {
                    int32_t diff = sa->original_z - sec->floorz;
                    if (klabs(diff) <= sa->speed) {
                        sec->floorz = sa->original_z;
                        sa->active = 0;
                    } else {
                        sec->floorz += (diff > 0) ? sa->speed : -sa->speed;
                    }
                } else {
                    sa->timer--;
                    if (sa->timer <= 0) {
                        sa->direction = -1;
                    }
                }
                break;
            }

            default:
                break;
        }
    }
}

void sectors_check_player(game_t *g) {
    if (g->player_sectnum < 0 || g->player_sectnum >= numsectors) return;

    sectortype *sec = &sector[g->player_sectnum];
    int16_t tag = sec->lotag;

    switch (tag) {
        case SECT_DAMAGE_LOW:
            /* Low damage every second */
            if ((g->ticcount % 26) == 0) {
                player_damage(g, 2);
            }
            break;

        case SECT_DAMAGE_HIGH:
            /* High damage */
            if ((g->ticcount % 13) == 0) {
                player_damage(g, 10);
            }
            break;

        case SECT_SECRET:
            /* Found a secret! Only count once */
            if (sec->hitag == 0) {
                g->secrets++;
                sec->hitag = 1; /* Mark as found */
            }
            break;

        case SECT_END_LEVEL:
            /* Level complete */
            g->state = GAMESTATE_VICTORY;
            break;

        case SECT_WATER:
            /* Water sector - apply bob effect */
            /* Player physics handles this via sector ceilingz/floorz */
            break;

        case SECT_TELEPORT: {
            /* Find destination sector (matching hitag) */
            int16_t dest_tag = sec->hitag;
            for (int s = 0; s < numsectors; s++) {
                if (s == g->player_sectnum) continue;
                if (sector[s].lotag == SECT_TELEPORT && sector[s].hitag == dest_tag) {
                    /* Teleport player to center of destination sector */
                    int w = sector[s].wallptr;
                    int32_t cx = 0, cy = 0;
                    int nw = sector[s].wallnum;
                    for (int j = w; j < w + nw; j++) {
                        cx += wall[j].x;
                        cy += wall[j].y;
                    }
                    if (nw > 0) {
                        g->player_x = cx / nw;
                        g->player_y = cy / nw;
                    }
                    g->player_z = sector[s].floorz - PLAYER_EYE_HEIGHT;
                    g->player_sectnum = s;
                    break;
                }
            }
            break;
        }

        default:
            break;
    }
}
