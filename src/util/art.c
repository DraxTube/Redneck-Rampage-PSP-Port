/*
 * art.c - ART tile file loader implementation
 * Redneck Rampage PSP Port
 */

#include "art.h"
#include "engine.h"
#include "cache.h"
#include "grp.h"

/* Tile pixel data storage */
static uint8_t *tile_rawdata[MAXTILES];

int art_load_file(const uint8_t *data, int32_t size) {
    if (!data || size < 16) return -1;

    const uint8_t *p = data;

    /* Read header */
    int32_t artversion    = read_le32(p); p += 4;
    int32_t numtiles_disc = read_le32(p); p += 4; /* unused/0 */
    int32_t localtilestart = read_le32(p); p += 4;
    int32_t localtileend   = read_le32(p); p += 4;

    (void)artversion;
    (void)numtiles_disc;

    if (localtilestart < 0 || localtileend < localtilestart)
        return -1;

    int32_t num_tiles = localtileend - localtilestart + 1;

    /* Check we have enough data for the header arrays */
    int32_t header_size = 16 + num_tiles * 2 + num_tiles * 2 + num_tiles * 4;
    if (size < header_size) return -1;

    /* Read tilesizx array */
    for (int i = 0; i < num_tiles; i++) {
        int tilenum = localtilestart + i;
        if (tilenum >= 0 && tilenum < MAXTILES) {
            tilesizx[tilenum] = read_le16(p);
        }
        p += 2;
    }

    /* Read tilesizy array */
    for (int i = 0; i < num_tiles; i++) {
        int tilenum = localtilestart + i;
        if (tilenum >= 0 && tilenum < MAXTILES) {
            tilesizy[tilenum] = read_le16(p);
        }
        p += 2;
    }

    /* Read picanm array */
    for (int i = 0; i < num_tiles; i++) {
        int tilenum = localtilestart + i;
        if (tilenum >= 0 && tilenum < MAXTILES) {
            picanm[tilenum] = read_le32u(p);
        }
        p += 4;
    }

    /* Read pixel data */
    for (int i = 0; i < num_tiles; i++) {
        int tilenum = localtilestart + i;
        if (tilenum < 0 || tilenum >= MAXTILES) continue;

        int32_t tw = tilesizx[tilenum];
        int32_t th = tilesizy[tilenum];
        int32_t tile_size = tw * th;

        if (tile_size <= 0) {
            tileptr[tilenum] = NULL;
            continue;
        }

        /* Check remaining data */
        int32_t remaining = size - (int32_t)(p - data);
        if (remaining < tile_size) {
            break;
        }

        /* Allocate and copy tile data */
        uint8_t *tile_data = (uint8_t *)xmalloc(tile_size);
        if (tile_data) {
            memcpy(tile_data, p, tile_size);
            tile_rawdata[tilenum] = tile_data;
            tileptr[tilenum] = tile_data;

            /* Add to cache */
            cache_insert(&g_tile_cache, tilenum, tile_data, tile_size);
        }
        p += tile_size;
    }

    return 0;
}

int art_load_from_grp(void) {
    if (!g_grp) return -1;

    char artname[32];
    int loaded = 0;

    for (int i = 0; i < ART_MAX_FILES; i++) {
        snprintf(artname, sizeof(artname), "TILES%03d.ART", i);

        int32_t art_size = 0;
        uint8_t *art_data = grp_read_file(g_grp, artname, &art_size);
        if (!art_data) continue;

        if (art_load_file(art_data, art_size) == 0) {
            loaded++;
        }

        free(art_data);
    }

    return loaded > 0 ? 0 : -1;
}

uint8_t *art_get_tile(int16_t tilenum) {
    if (tilenum < 0 || tilenum >= MAXTILES) return NULL;
    return tileptr[tilenum];
}

void art_get_tile_size(int16_t tilenum, int16_t *w, int16_t *h) {
    if (tilenum < 0 || tilenum >= MAXTILES) {
        if (w) *w = 0;
        if (h) *h = 0;
        return;
    }
    if (w) *w = tilesizx[tilenum];
    if (h) *h = tilesizy[tilenum];
}

uint32_t art_get_picanm(int16_t tilenum) {
    if (tilenum < 0 || tilenum >= MAXTILES) return 0;
    return picanm[tilenum];
}

int art_tile_exists(int16_t tilenum) {
    if (tilenum < 0 || tilenum >= MAXTILES) return 0;
    return (tilesizx[tilenum] > 0 && tilesizy[tilenum] > 0);
}
