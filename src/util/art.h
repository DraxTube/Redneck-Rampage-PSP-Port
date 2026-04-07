/*
 * art.h - ART tile file format
 * Redneck Rampage PSP Port
 *
 * ART files contain the tile/texture graphics for Build engine games.
 * Tiles are stored as 8-bit palette-indexed pixels in column-major order.
 */

#ifndef ART_H
#define ART_H

#include "compat.h"

#define ART_VERSION     1
#define ART_MAX_FILES   20    /* TILES000.ART through TILES019.ART */

/* Load all ART files from the GRP archive */
int art_load_from_grp(void);

/* Load a specific ART file */
int art_load_file(const uint8_t *data, int32_t size);

/* Get raw tile pixel data (column-major, palette-indexed) */
uint8_t *art_get_tile(int16_t tilenum);

/* Get tile dimensions */
void art_get_tile_size(int16_t tilenum, int16_t *w, int16_t *h);

/* Get animation info for a tile */
uint32_t art_get_picanm(int16_t tilenum);

/* Check if tile has data */
int art_tile_exists(int16_t tilenum);

#endif /* ART_H */
