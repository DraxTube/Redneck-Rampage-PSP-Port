/*
 * cache.h - Tile caching system
 * Redneck Rampage PSP Port
 */

#ifndef CACHE_H
#define CACHE_H

#include "compat.h"

#define CACHE_MAX_SIZE  (8 * 1024 * 1024)  /* 8MB tile cache limit */

typedef struct cache_entry {
    uint8_t  *data;
    int32_t   size;
    int32_t   tile_num;
    int32_t   lru_stamp;
    struct cache_entry *next;
} cache_entry_t;

typedef struct {
    cache_entry_t *entries;
    int32_t        num_entries;
    int32_t        total_size;
    int32_t        max_size;
    int32_t        lru_counter;
} tile_cache_t;

/* Initialize the tile cache */
void  cache_init(tile_cache_t *cache, int32_t max_size);

/* Free all cache entries */
void  cache_free(tile_cache_t *cache);

/* Add tile data to cache, evicting LRU entries if needed */
void  cache_insert(tile_cache_t *cache, int32_t tile_num,
                   uint8_t *data, int32_t size);

/* Look up tile in cache; returns data or NULL */
uint8_t *cache_lookup(tile_cache_t *cache, int32_t tile_num);

/* Evict least-recently-used entries until size < target */
void  cache_evict(tile_cache_t *cache, int32_t target_free);

/* Global tile cache instance */
extern tile_cache_t g_tile_cache;

#endif /* CACHE_H */
