/*
 * cache.c - Tile caching system implementation
 * Redneck Rampage PSP Port
 */

#include "cache.h"

tile_cache_t g_tile_cache;

void cache_init(tile_cache_t *cache, int32_t max_size) {
    memset(cache, 0, sizeof(tile_cache_t));
    cache->max_size = max_size;
    cache->lru_counter = 0;
    cache->entries = NULL;
    cache->num_entries = 0;
    cache->total_size = 0;
}

void cache_free(tile_cache_t *cache) {
    cache_entry_t *e = cache->entries;
    while (e) {
        cache_entry_t *next = e->next;
        if (e->data) free(e->data);
        free(e);
        e = next;
    }
    cache->entries = NULL;
    cache->num_entries = 0;
    cache->total_size = 0;
}

uint8_t *cache_lookup(tile_cache_t *cache, int32_t tile_num) {
    cache_entry_t *e = cache->entries;
    while (e) {
        if (e->tile_num == tile_num) {
            e->lru_stamp = ++cache->lru_counter;
            return e->data;
        }
        e = e->next;
    }
    return NULL;
}

void cache_evict(tile_cache_t *cache, int32_t target_free) {
    while (cache->total_size + target_free > cache->max_size && cache->entries) {
        /* Find LRU entry */
        cache_entry_t *lru = cache->entries;
        cache_entry_t *lru_prev = NULL;
        cache_entry_t *prev = NULL;
        cache_entry_t *e = cache->entries;

        while (e) {
            if (e->lru_stamp < lru->lru_stamp) {
                lru = e;
                lru_prev = prev;
            }
            prev = e;
            e = e->next;
        }

        /* Remove LRU entry */
        if (lru_prev) {
            lru_prev->next = lru->next;
        } else {
            cache->entries = lru->next;
        }

        cache->total_size -= lru->size;
        cache->num_entries--;
        if (lru->data) free(lru->data);
        free(lru);
    }
}

void cache_insert(tile_cache_t *cache, int32_t tile_num, uint8_t *data, int32_t size) {
    /* Check if already cached */
    cache_entry_t *e = cache->entries;
    while (e) {
        if (e->tile_num == tile_num) {
            e->lru_stamp = ++cache->lru_counter;
            return; /* Already cached */
        }
        e = e->next;
    }

    /* Evict if needed */
    if (cache->total_size + size > cache->max_size) {
        cache_evict(cache, size);
    }

    /* Create new entry */
    cache_entry_t *entry = (cache_entry_t *)malloc(sizeof(cache_entry_t));
    if (!entry) return;

    entry->data = (uint8_t *)malloc(size);
    if (!entry->data) {
        free(entry);
        return;
    }

    memcpy(entry->data, data, size);
    entry->size = size;
    entry->tile_num = tile_num;
    entry->lru_stamp = ++cache->lru_counter;
    entry->next = cache->entries;
    cache->entries = entry;
    cache->num_entries++;
    cache->total_size += size;
}
