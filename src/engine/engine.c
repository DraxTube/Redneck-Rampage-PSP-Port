/*
 * engine.c - Build Engine software renderer
 * Redneck Rampage PSP Port
 *
 * Implements the core Build engine raycasting renderer with portal-based
 * visibility determination, textured walls, flat-shaded floors/ceilings,
 * and billboard sprite rendering. Adapted for PSP's 320x200 framebuffer.
 */

#include "engine.h"
#include "cache.h"
#include "grp.h"
#include "art.h"
#include "map.h"
#include <math.h>

/* ============================================================
 * Global engine data (dynamically allocated)
 * ============================================================ */
sectortype  *sector = NULL;
walltype    *wall = NULL;
spritetype  *sprite = NULL;
int16_t     numsectors = 0, numwalls = 0;
int16_t     numsprites = 0;

spritelisttype sprlist;

int16_t     *tilesizx = NULL;
int16_t     *tilesizy = NULL;
uint32_t    *picanm = NULL;
uint8_t    **tileptr = NULL;

uint8_t     palette_raw[768];
uint32_t    palette32[256];
uint8_t     palookup[64][256];
int32_t     numshades = 32;

int32_t     sintable[2048 + 512];

uint32_t    *framebuffer = NULL;

/* ============================================================
 * Renderer state (per-frame)
 * ============================================================ */
static int32_t  globalposx, globalposy, globalposz;
static int16_t  globalang, globalhoriz, globalsectnum;
static int32_t  globalcosx, globalsinx;

/* Per-column clipping */
static int16_t  umost[XDIM];   /* Upper clip boundary */
static int16_t  dmost[XDIM];   /* Lower clip boundary */

/* Sector rendering queue */
#define MAX_RENDER_SECTORS 256
typedef struct {
    int16_t sectnum;
    int16_t sx1, sx2;           /* Screen column range */
} render_sector_t;

static render_sector_t render_queue[MAX_RENDER_SECTORS];
static int render_queue_head, render_queue_tail;
static uint8_t sector_rendered[MAXSECTORS / 8 + 1];

/* Sprite collection for deferred rendering */
#define MAX_TSPRITES 512
static tsprtype tsprite[MAX_TSPRITES];
static int32_t  num_tsprites;

/* Focal length for perspective projection */
#define FOCAL_LENGTH  (HALFXDIM)

/* ============================================================
 * Initialization
 * ============================================================ */
void initengine(void) {
    int i;

    /* Dynamically allocate large arrays */
    sector     = (sectortype *)calloc(MAXSECTORS, sizeof(sectortype));
    wall       = (walltype *)calloc(MAXWALLS, sizeof(walltype));
    sprite     = (spritetype *)calloc(MAXSPRITES, sizeof(spritetype));
    tilesizx   = (int16_t *)calloc(MAXTILES, sizeof(int16_t));
    tilesizy   = (int16_t *)calloc(MAXTILES, sizeof(int16_t));
    picanm     = (uint32_t *)calloc(MAXTILES, sizeof(uint32_t));
    tileptr    = (uint8_t **)calloc(MAXTILES, sizeof(uint8_t *));
    framebuffer = (uint32_t *)calloc(XDIM * YDIM, sizeof(uint32_t));

    if (!sector || !wall || !sprite || !tilesizx || !tilesizy ||
        !picanm || !tileptr || !framebuffer) {
        /* Fatal: can't allocate core arrays */
        return;
    }

    /* Build sine table (16.16 fixed-point) */
    for (i = 0; i < 2048; i++) {
        float angle = (float)i * (2.0f * 3.14159265f / 2048.0f);
        sintable[i] = (int32_t)(sinf(angle) * 65536.0f);
    }
    /* Copy for cosine lookups: cos(x) = sin(x + 512) */
    for (i = 0; i < 512; i++) {
        sintable[2048 + i] = sintable[i];
    }

    /* Initialize sprite lists */
    memset(&sprlist, -1, sizeof(sprlist));
    for (i = 0; i < MAXSTATUS + 1; i++)
        sprlist.headspritestat[i] = -1;
    for (i = 0; i < MAXSECTORS + 1; i++)
        sprlist.headspritesect[i] = -1;

    /* Initialize cache */
    cache_init(&g_tile_cache, CACHE_MAX_SIZE);
}

void uninitengine(void) {
    cache_free(&g_tile_cache);

    free(sector);     sector = NULL;
    free(wall);       wall = NULL;
    free(sprite);     sprite = NULL;
    free(tilesizx);   tilesizx = NULL;
    free(tilesizy);   tilesizy = NULL;
    free(picanm);     picanm = NULL;
    free(tileptr);    tileptr = NULL;
    free(framebuffer); framebuffer = NULL;
}

/* ============================================================
 * Palette loading
 * ============================================================ */
int loadpalette(const char *palfile, const char *lookupfile) {
    int32_t pal_size = 0, lut_size = 0;
    uint8_t *pal_data = NULL, *lut_data = NULL;
    int i;

    /* Load PALETTE.DAT */
    pal_data = grp_read_file(g_grp, palfile, &pal_size);
    if (!pal_data || pal_size < 768) {
        if (pal_data) free(pal_data);
        return -1;
    }

    /* Copy raw palette (6-bit per component) */
    memcpy(palette_raw, pal_data, 768);

    /* Convert to ABGR8888 for PSP */
    for (i = 0; i < 256; i++) {
        uint8_t r = (pal_data[i * 3 + 0] & 0x3F) << 2;
        uint8_t g = (pal_data[i * 3 + 1] & 0x3F) << 2;
        uint8_t b = (pal_data[i * 3 + 2] & 0x3F) << 2;
        /* PSP ABGR8888: A=bits31-24, B=23-16, G=15-8, R=7-0 */
        palette32[i] = 0xFF000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    }

    /* Palette index 255 is typically transparent */
    palette32[255] = 0x00000000u;

    free(pal_data);

    /* Load LOOKUP.DAT (shade/remap tables) */
    lut_data = grp_read_file(g_grp, lookupfile, &lut_size);
    if (lut_data && lut_size >= 1) {
        numshades = (int32_t)lut_data[0];
        if (numshades > 64) numshades = 64;

        int32_t table_size = numshades * 256;
        if (lut_size >= 1 + table_size) {
            memcpy(palookup, lut_data + 1, table_size);
        } else {
            /* Generate basic shade tables */
            for (int s = 0; s < numshades; s++) {
                for (i = 0; i < 256; i++) {
                    int shade_factor = 256 - (s * 256 / numshades);
                    int r = ((palette_raw[i*3+0] << 2) * shade_factor) >> 8;
                    int g = ((palette_raw[i*3+1] << 2) * shade_factor) >> 8;
                    int b = ((palette_raw[i*3+2] << 2) * shade_factor) >> 8;
                    /* Find closest palette entry */
                    int best = 0, best_dist = 999999;
                    for (int j = 0; j < 256; j++) {
                        int dr = (palette_raw[j*3+0]<<2) - r;
                        int dg = (palette_raw[j*3+1]<<2) - g;
                        int db = (palette_raw[j*3+2]<<2) - b;
                        int dist = dr*dr + dg*dg + db*db;
                        if (dist < best_dist) { best_dist = dist; best = j; }
                    }
                    palookup[s][i] = (uint8_t)best;
                }
            }
        }
        free(lut_data);
    } else {
        /* No lookup table - generate basic */
        numshades = 32;
        for (int s = 0; s < numshades; s++) {
            for (i = 0; i < 256; i++) {
                palookup[s][i] = (uint8_t)i;
            }
        }
    }

    return 0;
}

/* ============================================================
 * Tile access
 * ============================================================ */
void loadtile(int16_t tilenum) {
    /* Tiles are loaded on ART load; this is a stub for on-demand */
    (void)tilenum;
}

uint8_t *get_tile_data(int16_t tilenum) {
    if (tilenum < 0 || tilenum >= MAXTILES) return NULL;
    return tileptr[tilenum];
}

/* ============================================================
 * Utility functions
 * ============================================================ */
int32_t ksqrt(int32_t val) {
    if (val <= 0) return 0;
    return (int32_t)sqrtf((float)val);
}

int32_t getangle(int32_t xvect, int32_t yvect) {
    if (xvect == 0 && yvect == 0) return 0;
    float angle = atan2f((float)yvect, (float)xvect);
    int32_t a = (int32_t)(angle * (1024.0f / 3.14159265f));
    return a & BUILDANG_MASK;
}

void rotatepoint(int32_t xpivot, int32_t ypivot, int32_t x, int32_t y,
                 int16_t ang, int32_t *ox, int32_t *oy) {
    int32_t dx = x - xpivot;
    int32_t dy = y - ypivot;
    int32_t cosval = sintable[(ang + 512) & BUILDANG_MASK];
    int32_t sinval = sintable[ang & BUILDANG_MASK];
    *ox = xpivot + mulscale16(dx, cosval) - mulscale16(dy, sinval);
    *oy = ypivot + mulscale16(dx, sinval) + mulscale16(dy, cosval);
}

/* ============================================================
 * Point-in-sector test
 * ============================================================ */
int inside(int32_t x, int32_t y, int16_t sectnum) {
    if (sectnum < 0 || sectnum >= numsectors) return 0;

    sectortype *sec = &sector[sectnum];
    int cnt = 0;
    int startwall = sec->wallptr;
    int endwall = startwall + sec->wallnum;

    for (int w = startwall; w < endwall; w++) {
        int32_t x1 = wall[w].x;
        int32_t y1 = wall[w].y;
        int32_t x2 = wall[wall[w].point2].x;
        int32_t y2 = wall[wall[w].point2].y;

        if ((y1 < y && y2 >= y) || (y2 < y && y1 >= y)) {
            if (x1 + mulscale16(y - y1, divscale16(x2 - x1, y2 - y1)) < x) {
                cnt ^= 1;
            }
        }
    }
    return cnt;
}

int updatesector(int32_t x, int32_t y, int16_t *sectnum) {
    if (*sectnum >= 0 && *sectnum < numsectors) {
        if (inside(x, y, *sectnum)) return 0;

        /* Check neighboring sectors */
        sectortype *sec = &sector[*sectnum];
        int sw = sec->wallptr;
        int ew = sw + sec->wallnum;
        for (int w = sw; w < ew; w++) {
            int16_t ns = wall[w].nextsector;
            if (ns >= 0 && inside(x, y, ns)) {
                *sectnum = ns;
                return 0;
            }
        }
    }

    /* Brute-force search */
    for (int16_t s = 0; s < numsectors; s++) {
        if (inside(x, y, s)) {
            *sectnum = s;
            return 0;
        }
    }
    return -1;
}

/* ============================================================
 * Z-range query (floor/ceiling heights at position)
 * ============================================================ */
void getzrange(int32_t x, int32_t y, int32_t z, int16_t sectnum,
               int32_t *ceilz, int32_t *florz, int32_t walldist) {
    (void)walldist;
    if (sectnum < 0 || sectnum >= numsectors) {
        *ceilz = 0;
        *florz = 0;
        return;
    }
    sectortype *sec = &sector[sectnum];

    /* Basic floor/ceiling (no slope for now) */
    *ceilz = sec->ceilingz;
    *florz = sec->floorz;

    /* Apply slope if enabled */
    if (sec->ceilingstat & SECSTAT_SLOPE) {
        int w = sec->wallptr;
        int32_t dx = wall[wall[w].point2].x - wall[w].x;
        int32_t dy = wall[wall[w].point2].y - wall[w].y;
        int32_t len = ksqrt(dx * dx + dy * dy);
        if (len > 0) {
            int32_t px = x - wall[w].x;
            int32_t py = y - wall[w].y;
            int32_t dot = mulscale16(px, dy) - mulscale16(py, dx);
            *ceilz += mulscale(sec->ceilingheinum, divscale16(dot, len), 16);
        }
    }
    if (sec->floorstat & SECSTAT_SLOPE) {
        int w = sec->wallptr;
        int32_t dx = wall[wall[w].point2].x - wall[w].x;
        int32_t dy = wall[wall[w].point2].y - wall[w].y;
        int32_t len = ksqrt(dx * dx + dy * dy);
        if (len > 0) {
            int32_t px = x - wall[w].x;
            int32_t py = y - wall[w].y;
            int32_t dot = mulscale16(px, dy) - mulscale16(py, dx);
            *florz += mulscale(sec->floorheinum, divscale16(dot, len), 16);
        }
    }

    (void)z;
}

/* ============================================================
 * Clip-move (collision with walls)
 * ============================================================ */
int clipmove(int32_t *x, int32_t *y, int32_t *z, int16_t *sectnum,
             int32_t xvect, int32_t yvect, int32_t walldist,
             int32_t ceildist, int32_t flordist) {
    int32_t newx = *x + (xvect >> 14);
    int32_t newy = *y + (yvect >> 14);
    int hit = 0;

    (void)ceildist;
    (void)flordist;
    (void)z;

    if (*sectnum < 0 || *sectnum >= numsectors) return 0;

    sectortype *sec = &sector[*sectnum];
    int sw = sec->wallptr;
    int ew = sw + sec->wallnum;

    for (int w = sw; w < ew; w++) {
        int32_t x1 = wall[w].x;
        int32_t y1 = wall[w].y;
        int32_t x2 = wall[wall[w].point2].x;
        int32_t y2 = wall[wall[w].point2].y;

        /* Wall normal direction */
        int32_t dx = x2 - x1;
        int32_t dy = y2 - y1;

        /* Distance from new position to wall line */
        int64_t num = (int64_t)(newy - y1) * dx - (int64_t)(newx - x1) * dy;
        int64_t den_sq = (int64_t)dx * dx + (int64_t)dy * dy;
        if (den_sq == 0) continue;

        int32_t den = ksqrt((int32_t)(den_sq >> 8));
        if (den == 0) continue;

        int32_t dist = (int32_t)(num / (int64_t)den) >> 4;

        if (klabs(dist) < walldist) {
            /* Check if we're on the wall segment */
            int64_t t_num = (int64_t)(newx - x1) * dx + (int64_t)(newy - y1) * dy;
            int32_t t = 0;
            if (den_sq > 0) t = (int32_t)((t_num * 1024) / den_sq);

            if (t >= -128 && t <= 1024 + 128) {
                int16_t ns = wall[w].nextsector;
                if (ns < 0 || (wall[w].cstat & WALLCSTAT_BLOCK)) {
                    /* Solid wall - slide along it */
                    int64_t slide_x = (int64_t)dx * ((int64_t)(xvect >> 14) * dx + (int64_t)(yvect >> 14) * dy);
                    int64_t slide_y = (int64_t)dy * ((int64_t)(xvect >> 14) * dx + (int64_t)(yvect >> 14) * dy);
                    if (den_sq > 0) {
                        newx = *x + (int32_t)(slide_x / den_sq);
                        newy = *y + (int32_t)(slide_y / den_sq);
                    }
                    hit = 1;
                }
            }
        }
    }

    *x = newx;
    *y = newy;

    /* Update sector */
    updatesector(newx, newy, sectnum);

    return hit;
}

/* ============================================================
 * Hitscan - trace a ray and find what it hits
 * ============================================================ */
int hitscan(int32_t xs, int32_t ys, int32_t zs, int16_t sectnum,
            int32_t vx, int32_t vy, int32_t vz, hitdata_t *hit) {
    hit->sect = -1;
    hit->wall = -1;
    hit->sprite = -1;
    hit->x = xs;
    hit->y = ys;
    hit->z = zs;

    if (sectnum < 0 || sectnum >= numsectors) return -1;

    int32_t best_dist = 0x7FFFFFFF;
    int16_t cur_sect = sectnum;
    int iterations = 0;

    while (cur_sect >= 0 && cur_sect < numsectors && iterations < 128) {
        iterations++;
        sectortype *sec = &sector[cur_sect];
        int sw = sec->wallptr;
        int ew = sw + sec->wallnum;
        int16_t next_sect = -1;

        /* Check walls */
        for (int w = sw; w < ew; w++) {
            int32_t x1 = wall[w].x - xs;
            int32_t y1 = wall[w].y - ys;
            int32_t x2 = wall[wall[w].point2].x - xs;
            int32_t y2 = wall[wall[w].point2].y - ys;

            int32_t d = vx * (y2 - y1) - vy * (x2 - x1);
            if (d == 0) continue;

            int64_t t_num = (int64_t)x1 * (y2 - y1) - (int64_t)y1 * (x2 - x1);
            int32_t t = (int32_t)((t_num * 4096) / d);

            if (t <= 0) continue;

            int64_t u_num = (int64_t)x1 * vy - (int64_t)y1 * vx;
            int32_t u = (int32_t)((u_num * 4096) / d);

            if (u < 0 || u > 4096) continue;

            if (t < best_dist) {
                int32_t hx = xs + (int32_t)((int64_t)vx * t / 4096);
                int32_t hy = ys + (int32_t)((int64_t)vy * t / 4096);
                int32_t hz = zs + (int32_t)((int64_t)vz * t / 4096);

                if (wall[w].nextsector >= 0 && !(wall[w].cstat & WALLCSTAT_BLOCK_HIT)) {
                    sectortype *nsec = &sector[wall[w].nextsector];
                    if (hz > nsec->ceilingz && hz < nsec->floorz) {
                        next_sect = wall[w].nextsector;
                        continue;
                    }
                }

                best_dist = t;
                hit->sect = cur_sect;
                hit->wall = w;
                hit->sprite = -1;
                hit->x = hx;
                hit->y = hy;
                hit->z = hz;
            }
        }

        /* Check floor/ceiling */
        if (vz != 0) {
            int32_t fz_dist = (int32_t)(((int64_t)(sec->floorz - zs) * 4096) / vz);
            if (fz_dist > 0 && fz_dist < best_dist) {
                int32_t hx = xs + (int32_t)((int64_t)vx * fz_dist / 4096);
                int32_t hy = ys + (int32_t)((int64_t)vy * fz_dist / 4096);
                if (inside(hx, hy, cur_sect)) {
                    best_dist = fz_dist;
                    hit->sect = cur_sect;
                    hit->wall = -1;
                    hit->sprite = -1;
                    hit->x = hx;
                    hit->y = hy;
                    hit->z = sec->floorz;
                }
            }
            int32_t cz_dist = (int32_t)(((int64_t)(sec->ceilingz - zs) * 4096) / vz);
            if (cz_dist > 0 && cz_dist < best_dist) {
                int32_t hx = xs + (int32_t)((int64_t)vx * cz_dist / 4096);
                int32_t hy = ys + (int32_t)((int64_t)vy * cz_dist / 4096);
                if (inside(hx, hy, cur_sect)) {
                    best_dist = cz_dist;
                    hit->sect = cur_sect;
                    hit->wall = -1;
                    hit->sprite = -1;
                    hit->x = hx;
                    hit->y = hy;
                    hit->z = sec->ceilingz;
                }
            }
        }

        /* Check sprites in sector */
        for (int s = 0; s < numsprites; s++) {
            if (sprite[s].sectnum != cur_sect) continue;
            if (sprite[s].cstat & SPRCSTAT_INVISIBLE) continue;

            int32_t dx = sprite[s].x - xs;
            int32_t dy = sprite[s].y - ys;
            int32_t dot = mulscale16(dx, vx) + mulscale16(dy, vy);
            if (dot <= 0) continue;

            int32_t vlen_sq = mulscale16(vx, vx) + mulscale16(vy, vy);
            if (vlen_sq == 0) continue;

            int32_t t = divscale(dot, ksqrt(vlen_sq), 12);
            if (t > 0 && t < best_dist) {
                int32_t cdist = klabs(mulscale16(dx, vy) - mulscale16(dy, vx));
                int32_t radius = (int32_t)sprite[s].clipdist << 2;
                if (radius < 64) radius = 64;

                if (cdist < radius) {
                    best_dist = t;
                    hit->sect = cur_sect;
                    hit->wall = -1;
                    hit->sprite = s;
                    hit->x = xs + (int32_t)((int64_t)vx * t / 4096);
                    hit->y = ys + (int32_t)((int64_t)vy * t / 4096);
                    hit->z = zs + (int32_t)((int64_t)vz * t / 4096);
                }
            }
        }

        if (next_sect >= 0) {
            cur_sect = next_sect;
            next_sect = -1;
        } else {
            break;
        }
    }

    return (hit->sect >= 0) ? 0 : -1;
}

/* ============================================================
 * Sprite management
 * ============================================================ */
int insertsprite(int16_t sectnum, int16_t statnum) {
    int16_t i;
    for (i = 0; i < MAXSPRITES; i++) {
        if (sprite[i].statnum == -1 || sprite[i].statnum == MAXSTATUS)
            break;
    }
    if (i >= MAXSPRITES) return -1;

    memset(&sprite[i], 0, sizeof(spritetype));
    sprite[i].sectnum = sectnum;
    sprite[i].statnum = statnum;
    sprite[i].xrepeat = 64;
    sprite[i].yrepeat = 64;
    sprite[i].clipdist = 32;
    sprite[i].owner = -1;

    if (i >= numsprites) numsprites = i + 1;
    return i;
}

int deletesprite(int16_t spritenum) {
    if (spritenum < 0 || spritenum >= MAXSPRITES) return -1;
    sprite[spritenum].statnum = MAXSTATUS;
    sprite[spritenum].sectnum = -1;
    return 0;
}

int changespritesect(int16_t spritenum, int16_t newsectnum) {
    if (spritenum < 0 || spritenum >= MAXSPRITES) return -1;
    sprite[spritenum].sectnum = newsectnum;
    return 0;
}

int changespritestat(int16_t spritenum, int16_t newstatnum) {
    if (spritenum < 0 || spritenum >= MAXSPRITES) return -1;
    sprite[spritenum].statnum = newstatnum;
    return 0;
}

/* ============================================================
 * Software Renderer: Wall column drawing
 * ============================================================ */
static void draw_wall_column(int x, int yt, int yb,
                             int16_t picnum, int tex_u,
                             int8_t shade, uint8_t pal_num) {
    (void)pal_num;
    if (x < 0 || x >= XDIM) return;

    /* Clip to column bounds */
    int y_top = kmax(yt, (int)umost[x]);
    int y_bot = kmin(yb, (int)dmost[x]);
    if (y_top > y_bot) return;

    uint8_t *tile = get_tile_data(picnum);
    int tw = tilesizx[picnum];
    int th = tilesizy[picnum];

    /* Shade index clamped */
    int shade_idx = kclamp((int)shade, 0, numshades - 1);

    if (!tile || tw <= 0 || th <= 0) {
        /* No tile - draw solid color */
        uint32_t color = palette32[shade_idx * 8];
        for (int y = y_top; y <= y_bot; y++) {
            framebuffer[y * XDIM + x] = color;
        }
        return;
    }

    /* Texture coordinate */
    int u = tex_u % tw;
    if (u < 0) u += tw;

    /* Tile column (column-major storage) */
    uint8_t *col_data = tile + u * th;

    /* Column height on screen */
    int col_height = yb - yt + 1;
    if (col_height <= 0) return;

    /* Vertical texture stepping (16.16 fixed-point) */
    int32_t v_step = (th << 16) / col_height;
    int32_t v_pos = (int32_t)(y_top - yt) * v_step;

    const uint8_t *shade_table = palookup[shade_idx];

    for (int y = y_top; y <= y_bot; y++) {
        int v = (v_pos >> 16) % th;
        if (v < 0) v += th;
        uint8_t pidx = col_data[v];
        if (pidx != 255) {
            uint8_t shaded = shade_table[pidx];
            framebuffer[y * XDIM + x] = palette32[shaded];
        }
        v_pos += v_step;
    }
}

/* ============================================================
 * Software Renderer: Floor/ceiling span (flat color + shade)
 * ============================================================ */
static void draw_flat_span(int x1, int x2, int y,
                           int16_t picnum, int8_t shade) {
    if (y < 0 || y >= YDIM) return;
    if (x1 < 0) x1 = 0;
    if (x2 >= XDIM) x2 = XDIM - 1;
    if (x1 > x2) return;

    int shade_idx = kclamp((int)shade, 0, numshades - 1);

    uint8_t *tile = get_tile_data(picnum);
    int tw = tilesizx[picnum];
    int th = tilesizy[picnum];

    if (!tile || tw <= 0 || th <= 0) {
        /* Solid color fallback */
        uint32_t color = palette32[kclamp(shade_idx * 4, 0, 255)];
        uint32_t *row = &framebuffer[y * XDIM];
        for (int x = x1; x <= x2; x++) {
            row[x] = color;
        }
        return;
    }

    /* Simple textured floor/ceiling with fixed pattern */
    const uint8_t *shade_table = palookup[shade_idx];
    uint32_t *row = &framebuffer[y * XDIM];

    /* Calculate world coordinates for floor texturing */
    int32_t dy = y - HALFYDIM - (globalhoriz - 100);
    if (dy == 0) dy = 1;

    for (int x = x1; x <= x2; x++) {
        int32_t dist = klabs(FOCAL_LENGTH * 64 / dy);
        int32_t dx_screen = x - HALFXDIM;

        int32_t world_x = globalposx + mulscale16(dist * dx_screen, globalcosx) +
                          mulscale16(dist * FOCAL_LENGTH, globalsinx);
        int32_t world_y = globalposy + mulscale16(dist * dx_screen, globalsinx) -
                          mulscale16(dist * FOCAL_LENGTH, globalcosx);

        int u = (world_x >> 4) & (tw - 1);
        int v = (world_y >> 4) & (th - 1);
        if (u < 0) u += tw;
        if (v < 0) v += th;

        uint8_t pidx = tile[u * th + v];
        if (pidx != 255) {
            row[x] = palette32[shade_table[pidx]];
        }
    }
}

/* ============================================================
 * Software Renderer: Process a sector's walls
 * ============================================================ */
static void render_sector(int16_t sectnum, int16_t clip_x1, int16_t clip_x2) {
    if (sectnum < 0 || sectnum >= numsectors) return;

    sectortype *sec = &sector[sectnum];
    int sw = sec->wallptr;
    int ew = sw + sec->wallnum;

    int32_t ceilz = sec->ceilingz;
    int32_t florz = sec->floorz;

    for (int w = sw; w < ew; w++) {
        walltype *wal = &wall[w];
        walltype *wal2 = &wall[wal->point2];

        /* Transform wall endpoints to view space */
        int32_t dx1 = wal->x - globalposx;
        int32_t dy1 = wal->y - globalposy;
        int32_t dx2 = wal2->x - globalposx;
        int32_t dy2 = wal2->y - globalposy;

        /* Rotate into view */
        int32_t vx1 = mulscale16(dx1, globalsinx) - mulscale16(dy1, globalcosx);
        int32_t vy1 = mulscale16(dx1, globalcosx) + mulscale16(dy1, globalsinx);

        int32_t vx2 = mulscale16(dx2, globalsinx) - mulscale16(dy2, globalcosx);
        int32_t vy2 = mulscale16(dx2, globalcosx) + mulscale16(dy2, globalsinx);

        /* Both behind near plane? */
        if (vy1 <= 16 && vy2 <= 16) continue;

        /* Clip to near plane */

        if (vy1 < 16) {
            int32_t t = divscale16(16 - vy1, vy2 - vy1);
            vx1 = vx1 + mulscale16(vx2 - vx1, t);
            vy1 = 16;
        }
        if (vy2 < 16) {
            int32_t t = divscale16(16 - vy2, vy1 - vy2);
            vx2 = vx2 + mulscale16(vx1 - vx2, t);
            vy2 = 16;
        }

        /* Perspective project to screen X */
        int32_t sx1 = HALFXDIM + (vx1 * FOCAL_LENGTH / vy1);
        int32_t sx2 = HALFXDIM + (vx2 * FOCAL_LENGTH / vy2);

        if (sx1 >= sx2) continue;     /* Back-facing */
        if (sx2 <= clip_x1 || sx1 >= clip_x2) continue; /* Out of clip range */

        /* Clip to screen/clip range */
        int32_t csx1 = kmax(sx1, (int32_t)clip_x1);
        int32_t csx2 = kmin(sx2, (int32_t)clip_x2);
        if (csx1 >= csx2) continue;

        /* Calculate wall Y coordinates (top/bottom) for ceiling and floor */
        int32_t horiz_offset = globalhoriz - 100;

        /* For each screen column in the wall */
        for (int32_t x = csx1; x < csx2; x++) {
            if (umost[x] >= dmost[x]) continue;

            /* Interpolation factor along wall */
            int32_t t;
            if (sx2 == sx1) t = 0;
            else t = divscale16(x - sx1, sx2 - sx1);

            /* Interpolate depth */
            int32_t iz1 = divscale16(1, vy1);
            int32_t iz2 = divscale16(1, vy2);
            int32_t iz = iz1 + mulscale16(iz2 - iz1, t);
            int32_t z_at_col;
            if (iz > 0) z_at_col = divscale16(1, iz);
            else z_at_col = 32768;

            /* Screen Y for ceiling and floor at this depth */
            int32_t ceil_sy = HALFYDIM - (int32_t)(((int64_t)(globalposz - ceilz) * FOCAL_LENGTH) / z_at_col) + horiz_offset;
            int32_t flor_sy = HALFYDIM - (int32_t)(((int64_t)(globalposz - florz) * FOCAL_LENGTH) / z_at_col) + horiz_offset;

            int32_t wall_ytop = kclamp(ceil_sy, 0, YDIM - 1);
            int32_t wall_ybot = kclamp(flor_sy, 0, YDIM - 1);

            /* Draw ceiling (from umost to ceil_sy) */
            if (ceil_sy > umost[x]) {
                int32_t cy1 = umost[x];
                int32_t cy2 = kmin(ceil_sy - 1, dmost[x]);
                if (cy1 <= cy2) {
                    for (int y = cy1; y <= cy2; y++) {
                        /* Simple flat ceiling color */
                        draw_flat_span(x, x, y, sec->ceilingpicnum, sec->ceilingshade);
                    }
                }
            }

            /* Draw floor (from flor_sy to dmost) */
            if (flor_sy < dmost[x]) {
                int32_t fy1 = kmax(flor_sy + 1, umost[x]);
                int32_t fy2 = dmost[x];
                if (fy1 <= fy2) {
                    for (int y = fy1; y <= fy2; y++) {
                        draw_flat_span(x, x, y, sec->floorpicnum, sec->floorshade);
                    }
                }
            }

            /* Texture U coordinate for this column */
            int32_t wall_dx = wal2->x - wal->x;
            int32_t wall_dy = wal2->y - wal->y;
            int32_t wall_len = ksqrt(wall_dx * wall_dx + wall_dy * wall_dy);
            int32_t tex_u_raw = mulscale16(t, wall_len);
            int32_t xrep = wal->xrepeat;
            if (xrep == 0) xrep = 8;
            int tex_u = (tex_u_raw * xrep / 8 + wal->xpanning * 8) >> 3;

            /* Shade based on distance */
            int32_t dist_shade = z_at_col >> 10;
            int8_t total_shade = (int8_t)kclamp((int)wal->shade + dist_shade, 0, numshades - 1);

            if (wal->nextsector < 0) {
                /* Solid wall */
                draw_wall_column(x, wall_ytop, wall_ybot, wal->picnum, tex_u,
                                total_shade, wal->pal);
                /* Mark column as fully drawn */
                umost[x] = (int16_t)(YDIM);
                dmost[x] = 0;
            } else {
                /* Portal wall - draw upper and lower parts */
                sectortype *nsec = &sector[wal->nextsector];
                int32_t nceilz = nsec->ceilingz;
                int32_t nflorz = nsec->floorz;

                int32_t nceil_sy = HALFYDIM - (int32_t)(((int64_t)(globalposz - nceilz) * FOCAL_LENGTH) / z_at_col) + horiz_offset;
                int32_t nflor_sy = HALFYDIM - (int32_t)(((int64_t)(globalposz - nflorz) * FOCAL_LENGTH) / z_at_col) + horiz_offset;

                /* Upper wall (between this ceiling and next ceiling) */
                if (nceilz < ceilz) {
                    int32_t uy_top = wall_ytop;
                    int32_t uy_bot = kclamp(nceil_sy, 0, YDIM - 1);
                    if (uy_top < uy_bot) {
                        draw_wall_column(x, uy_top, uy_bot, wal->picnum, tex_u,
                                        total_shade, wal->pal);
                    }
                }

                /* Lower wall (between next floor and this floor) */
                if (nflorz > florz) {
                    int32_t ly_top = kclamp(nflor_sy, 0, YDIM - 1);
                    int32_t ly_bot = wall_ybot;
                    if (ly_top < ly_bot) {
                        int16_t bottex = wal->picnum;
                        if (wal->cstat & WALLCSTAT_BOTTOMSWAP) {
                            walltype *nwal = &wall[wal->nextwall];
                            bottex = nwal->picnum;
                        }
                        draw_wall_column(x, ly_top, ly_bot, bottex, tex_u,
                                        total_shade, wal->pal);
                    }
                }

                /* Masked / transparent middle wall */
                if (wal->cstat & WALLCSTAT_MASKING) {
                    int32_t mt = kmax(nceil_sy, wall_ytop);
                    int32_t mb = kmin(nflor_sy, wall_ybot);
                    if (mt < mb) {
                        draw_wall_column(x, mt, mb, wal->overpicnum, tex_u,
                                        total_shade, wal->pal);
                    }
                }

                /* Update clipping for portal opening */
                int32_t new_top = kmax((int32_t)umost[x], nceil_sy);
                int32_t new_bot = kmin((int32_t)dmost[x], nflor_sy);
                umost[x] = (int16_t)kclamp(new_top, 0, YDIM);
                dmost[x] = (int16_t)kclamp(new_bot, 0, YDIM);
            }
        }

        /* Enqueue portal sector */
        if (wal->nextsector >= 0) {
            int byte_idx = wal->nextsector >> 3;
            int bit_idx = wal->nextsector & 7;
            if (!(sector_rendered[byte_idx] & (1 << bit_idx))) {
                sector_rendered[byte_idx] |= (1 << bit_idx);
                if (render_queue_tail < MAX_RENDER_SECTORS) {
                    render_queue[render_queue_tail].sectnum = wal->nextsector;
                    render_queue[render_queue_tail].sx1 = (int16_t)csx1;
                    render_queue[render_queue_tail].sx2 = (int16_t)csx2;
                    render_queue_tail++;
                }
            }
        }
    }

    /* Collect sprites in this sector for deferred rendering */
    for (int s = 0; s < numsprites; s++) {
        if (sprite[s].sectnum != sectnum) continue;
        if (sprite[s].cstat & SPRCSTAT_INVISIBLE) continue;
        if (sprite[s].statnum == MAXSTATUS) continue;
        if (num_tsprites >= MAX_TSPRITES) break;

        int32_t dx = sprite[s].x - globalposx;
        int32_t dy = sprite[s].y - globalposy;

        /* Transform to view space */
        int32_t viewx = mulscale16(dx, globalsinx) - mulscale16(dy, globalcosx);
        int32_t viewy = mulscale16(dx, globalcosx) + mulscale16(dy, globalsinx);

        if (viewy <= 16) continue; /* Behind camera */

        /* Screen position */
        int32_t scr_x = HALFXDIM + viewx * FOCAL_LENGTH / viewy;
        int32_t scr_y = HALFYDIM - (int32_t)(((int64_t)(globalposz - sprite[s].z) * FOCAL_LENGTH) / viewy)
                        + (globalhoriz - 100);

        tsprite[num_tsprites].sx = scr_x;
        tsprite[num_tsprites].sy = scr_y;
        tsprite[num_tsprites].dist = viewy;
        tsprite[num_tsprites].sprite_idx = s;
        tsprite[num_tsprites].owner = sprite[s].owner;
        num_tsprites++;
    }
}

/* ============================================================
 * Software Renderer: Sprite drawing (billboard)
 * ============================================================ */
static void draw_sprite(tsprtype *ts) {
    int16_t si = ts->sprite_idx;
    spritetype *spr = &sprite[si];
    int16_t picnum = spr->picnum;

    uint8_t *tile = get_tile_data(picnum);
    int tw = tilesizx[picnum];
    int th = tilesizy[picnum];
    if (!tile || tw <= 0 || th <= 0) return;

    /* Calculate sprite size on screen */
    int32_t xrep = spr->xrepeat;
    int32_t yrep = spr->yrepeat;
    if (xrep == 0) xrep = 32;
    if (yrep == 0) yrep = 32;

    int32_t spr_width = tw * xrep / 8;
    int32_t spr_height = th * yrep / 8;

    /* Screen dimensions at this distance */
    if (ts->dist <= 0) return;
    int32_t screen_w = spr_width * FOCAL_LENGTH / ts->dist;
    int32_t screen_h = spr_height * FOCAL_LENGTH / ts->dist;
    if (screen_w <= 0 || screen_h <= 0) return;
    if (screen_w > XDIM * 4) screen_w = XDIM * 4;
    if (screen_h > YDIM * 4) screen_h = YDIM * 4;

    /* Screen rectangle */
    int32_t cx = ts->sx - screen_w / 2 + mulscale16(spr->xoffset * xrep, FOCAL_LENGTH) / ts->dist;
    int32_t cy = ts->sy - screen_h + mulscale16(spr->yoffset * yrep, FOCAL_LENGTH) / ts->dist;

    /* Clip to screen */
    int32_t sx1 = kmax(cx, 0);
    int32_t sy1 = kmax(cy, 0);
    int32_t sx2 = kmin(cx + screen_w, XDIM);
    int32_t sy2 = kmin(cy + screen_h, YDIM);
    if (sx1 >= sx2 || sy1 >= sy2) return;

    /* Shade */
    int shade_idx = kclamp((int)spr->shade + (ts->dist >> 12), 0, numshades - 1);
    const uint8_t *shade_table = palookup[shade_idx];

    /* X-flip check */
    int xflip = (spr->cstat & SPRCSTAT_XFLIP) ? 1 : 0;
    int yflip = (spr->cstat & SPRCSTAT_YFLIP) ? 1 : 0;

    /* Draw sprite pixels */
    for (int32_t sy = sy1; sy < sy2; sy++) {
        int32_t tv_raw = (sy - cy) * th / screen_h;
        int tv = yflip ? (th - 1 - tv_raw) : tv_raw;
        if (tv < 0 || tv >= th) continue;

        for (int32_t sx = sx1; sx < sx2; sx++) {
            int32_t tu_raw = (sx - cx) * tw / screen_w;
            int tu = xflip ? (tw - 1 - tu_raw) : tu_raw;
            if (tu < 0 || tu >= tw) continue;

            /* Column-major: data[u * height + v] */
            uint8_t pidx = tile[tu * th + tv];
            if (pidx == 255) continue; /* Transparent */

            uint8_t shaded = shade_table[pidx];
            framebuffer[sy * XDIM + sx] = palette32[shaded];
        }
    }
}

/* ============================================================
 * Sort comparison for sprites (back-to-front)
 * ============================================================ */
static int tsprite_cmp(const void *a, const void *b) {
    const tsprtype *sa = (const tsprtype *)a;
    const tsprtype *sb = (const tsprtype *)b;
    if (sa->dist > sb->dist) return -1;
    if (sa->dist < sb->dist) return 1;
    return 0;
}

/* ============================================================
 * drawmasks - Draw sprites sorted back-to-front
 * ============================================================ */
void drawmasks(void) {
    if (num_tsprites <= 0) return;

    /* Sort back-to-front */
    qsort(tsprite, num_tsprites, sizeof(tsprtype), tsprite_cmp);

    /* Draw each sprite */
    for (int i = 0; i < num_tsprites; i++) {
        draw_sprite(&tsprite[i]);
    }
}

/* ============================================================
 * drawrooms - Main rendering entry point
 * ============================================================ */
void drawrooms(int32_t posx, int32_t posy, int32_t posz,
               int16_t ang, int16_t horiz, int16_t sectnum) {
    int x;

    if (sectnum < 0 || sectnum >= numsectors) return;

    /* Store global state */
    globalposx = posx;
    globalposy = posy;
    globalposz = posz;
    globalang = ang;
    globalhoriz = horiz;
    globalsectnum = sectnum;

    /* Precompute trig */
    globalcosx = sintable[(ang + 512) & BUILDANG_MASK]; /* cos(ang) * 65536 */
    globalsinx = sintable[ang & BUILDANG_MASK];         /* sin(ang) * 65536 */

    /* Initialize per-column clipping */
    for (x = 0; x < XDIM; x++) {
        umost[x] = 0;
        dmost[x] = YDIM - 1;
    }

    /* Clear framebuffer to dark blue (sky color) */
    for (int i = 0; i < XDIM * YDIM; i++) {
        framebuffer[i] = 0xFF200808; /* Dark sky color (ABGR) */
    }

    /* Reset sprite collector */
    num_tsprites = 0;

    /* Reset sector queue */
    render_queue_head = 0;
    render_queue_tail = 0;
    memset(sector_rendered, 0, sizeof(sector_rendered));

    /* Enqueue starting sector */
    render_queue[render_queue_tail].sectnum = sectnum;
    render_queue[render_queue_tail].sx1 = 0;
    render_queue[render_queue_tail].sx2 = XDIM;
    render_queue_tail++;
    sector_rendered[sectnum >> 3] |= (1 << (sectnum & 7));

    /* BFS through portals */
    while (render_queue_head < render_queue_tail) {
        render_sector_t *rs = &render_queue[render_queue_head++];
        render_sector(rs->sectnum, rs->sx1, rs->sx2);
    }

    /* Draw sprites */
    drawmasks();
}

/* ============================================================
 * Board loading wrapper
 * ============================================================ */
int loadboard(const char *filename, int32_t *posx, int32_t *posy,
              int32_t *posz, int16_t *ang, int16_t *sectnum) {
    return map_load(filename, posx, posy, posz, ang, sectnum);
}
