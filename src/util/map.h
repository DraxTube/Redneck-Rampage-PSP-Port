/*
 * map.h - Build engine MAP file format
 * Redneck Rampage PSP Port
 */

#ifndef MAP_H
#define MAP_H

#include "compat.h"

#define MAP_VERSION  7

/* Load a map from raw data (from GRP) */
int map_load_from_data(const uint8_t *data, int32_t size,
                       int32_t *posx, int32_t *posy, int32_t *posz,
                       int16_t *ang, int16_t *sectnum);

/* Load a map by name from the GRP archive */
int map_load(const char *name,
             int32_t *posx, int32_t *posy, int32_t *posz,
             int16_t *ang, int16_t *sectnum);

#endif /* MAP_H */
