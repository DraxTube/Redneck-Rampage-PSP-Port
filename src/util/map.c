/*
 * map.c - Build engine MAP file loader
 * Redneck Rampage PSP Port
 *
 * Build MAP format (version 7):
 *   mapversion (4 bytes)
 *   posx, posy, posz (4 bytes each)
 *   ang, cursectnum (2 bytes each)
 *   numsectors (2 bytes) + sector data
 *   numwalls (2 bytes) + wall data
 *   numsprites (2 bytes) + sprite data
 */

#include "map.h"
#include "engine.h"
#include "grp.h"

/* Size of serialized structures */
#define SECTOR_ONDISK_SIZE  40
#define WALL_ONDISK_SIZE    32
#define SPRITE_ONDISK_SIZE  44

int map_load_from_data(const uint8_t *data, int32_t size,
                       int32_t *posx, int32_t *posy, int32_t *posz,
                       int16_t *ang, int16_t *sectnum) {
    if (!data || size < 22) return -1;

    const uint8_t *p = data;

    /* Header */
    int32_t version = read_le32(p); p += 4;
    if (version != MAP_VERSION && version != 8 && version != 9) {
        return -1;
    }

    *posx     = read_le32(p);  p += 4;
    *posy     = read_le32(p);  p += 4;
    *posz     = read_le32(p);  p += 4;
    *ang      = read_le16(p);  p += 2;
    *sectnum  = read_le16(p);  p += 2;

    /* ---- Sectors ---- */
    numsectors = read_le16(p); p += 2;
    if (numsectors < 0 || numsectors > MAXSECTORS) return -1;

    for (int i = 0; i < numsectors; i++) {
        if ((int32_t)(p - data) + SECTOR_ONDISK_SIZE > size) return -1;

        sector[i].wallptr         = read_le16(p + 0);
        sector[i].wallnum         = read_le16(p + 2);
        sector[i].ceilingz        = read_le32(p + 4);
        sector[i].floorz          = read_le32(p + 8);
        sector[i].ceilingstat     = read_le16(p + 12);
        sector[i].floorstat       = read_le16(p + 14);
        sector[i].ceilingpicnum   = read_le16(p + 16);
        sector[i].ceilingheinum   = read_le16(p + 18);
        sector[i].ceilingshade    = (int8_t)p[20];
        sector[i].ceilingpal      = p[21];
        sector[i].ceilingxpanning = p[22];
        sector[i].ceilingypanning = p[23];
        sector[i].floorpicnum     = read_le16(p + 24);
        sector[i].floorheinum     = read_le16(p + 26);
        sector[i].floorshade      = (int8_t)p[28];
        sector[i].floorpal        = p[29];
        sector[i].floorxpanning   = p[30];
        sector[i].floorypanning   = p[31];
        sector[i].visibility      = p[32];
        sector[i].filler          = p[33];
        sector[i].lotag           = read_le16(p + 34);
        sector[i].hitag           = read_le16(p + 36);
        sector[i].extra           = read_le16(p + 38);

        p += SECTOR_ONDISK_SIZE;
    }

    /* ---- Walls ---- */
    numwalls = read_le16(p); p += 2;
    if (numwalls < 0 || numwalls > MAXWALLS) return -1;

    for (int i = 0; i < numwalls; i++) {
        if ((int32_t)(p - data) + WALL_ONDISK_SIZE > size) return -1;

        wall[i].x          = read_le32(p + 0);
        wall[i].y          = read_le32(p + 4);
        wall[i].point2     = read_le16(p + 8);
        wall[i].nextwall   = read_le16(p + 10);
        wall[i].nextsector = read_le16(p + 12);
        wall[i].cstat      = read_le16(p + 14);
        wall[i].picnum     = read_le16(p + 16);
        wall[i].overpicnum = read_le16(p + 18);
        wall[i].shade      = (int8_t)p[20];
        wall[i].pal        = p[21];
        wall[i].xrepeat    = p[22];
        wall[i].yrepeat    = p[23];
        wall[i].xpanning   = p[24];
        wall[i].ypanning   = p[25];
        wall[i].lotag      = read_le16(p + 26);
        wall[i].hitag      = read_le16(p + 28);
        wall[i].extra      = read_le16(p + 30);

        p += WALL_ONDISK_SIZE;
    }

    /* ---- Sprites ---- */
    numsprites = read_le16(p); p += 2;
    if (numsprites < 0 || numsprites > MAXSPRITES) numsprites = 0;

    for (int i = 0; i < numsprites; i++) {
        if ((int32_t)(p - data) + SPRITE_ONDISK_SIZE > size) break;

        sprite[i].x        = read_le32(p + 0);
        sprite[i].y        = read_le32(p + 4);
        sprite[i].z        = read_le32(p + 8);
        sprite[i].cstat    = read_le16(p + 12);
        sprite[i].picnum   = read_le16(p + 14);
        sprite[i].shade    = (int8_t)p[16];
        sprite[i].pal      = p[17];
        sprite[i].clipdist = p[18];
        sprite[i].filler   = p[19];
        sprite[i].xrepeat  = p[20];
        sprite[i].yrepeat  = p[21];
        sprite[i].xoffset  = (int8_t)p[22];
        sprite[i].yoffset  = (int8_t)p[23];
        sprite[i].sectnum  = read_le16(p + 24);
        sprite[i].statnum  = read_le16(p + 26);
        sprite[i].ang      = read_le16(p + 28);
        sprite[i].owner    = read_le16(p + 30);
        sprite[i].xvel     = read_le16(p + 32);
        sprite[i].yvel     = read_le16(p + 34);
        sprite[i].zvel     = read_le16(p + 36);
        sprite[i].lotag    = read_le16(p + 38);
        sprite[i].hitag    = read_le16(p + 40);
        sprite[i].extra    = read_le16(p + 42);

        p += SPRITE_ONDISK_SIZE;
    }

    /* Validate sector number */
    if (*sectnum < 0 || *sectnum >= numsectors) {
        *sectnum = 0;
    }

    return 0;
}

int map_load(const char *name,
             int32_t *posx, int32_t *posy, int32_t *posz,
             int16_t *ang, int16_t *sectnum) {
    if (!g_grp || !name) return -1;

    int32_t map_size = 0;
    uint8_t *map_data = grp_read_file(g_grp, name, &map_size);
    if (!map_data) return -1;

    int result = map_load_from_data(map_data, map_size, posx, posy, posz, ang, sectnum);
    free(map_data);

    return result;
}
