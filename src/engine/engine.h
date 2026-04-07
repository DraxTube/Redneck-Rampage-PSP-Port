/*
 * engine.h - Build Engine data structures and renderer API
 * Redneck Rampage PSP Port
 *
 * Implements Ken Silverman's Build engine data structures and
 * the software raycasting renderer adapted for PSP hardware.
 */

#ifndef ENGINE_H
#define ENGINE_H

#include "compat.h"

/* ============================================================
 * Build engine map structures
 * ============================================================ */

/* Sector - a 2D polygon with floor and ceiling */
typedef struct {
    int16_t  wallptr;           /* Index of first wall */
    int16_t  wallnum;           /* Number of walls */
    int32_t  ceilingz;          /* Ceiling height (z increases downward) */
    int32_t  floorz;            /* Floor height */
    int16_t  ceilingstat;       /* Ceiling rendering flags */
    int16_t  floorstat;         /* Floor rendering flags */
    int16_t  ceilingpicnum;     /* Ceiling tile number */
    int16_t  ceilingheinum;     /* Ceiling slope */
    int8_t   ceilingshade;      /* Ceiling shade level */
    uint8_t  ceilingpal;        /* Ceiling palette */
    uint8_t  ceilingxpanning;   /* Ceiling texture X offset */
    uint8_t  ceilingypanning;   /* Ceiling texture Y offset */
    int16_t  floorpicnum;       /* Floor tile number */
    int16_t  floorheinum;       /* Floor slope */
    int8_t   floorshade;        /* Floor shade level */
    uint8_t  floorpal;          /* Floor palette */
    uint8_t  floorxpanning;     /* Floor texture X offset */
    uint8_t  floorypanning;     /* Floor texture Y offset */
    uint8_t  visibility;        /* Visibility/fog distance */
    uint8_t  filler;            /* Alignment padding */
    int16_t  lotag;             /* Low tag (game use) */
    int16_t  hitag;             /* High tag (game use) */
    int16_t  extra;             /* Extra data (game use) */
} sectortype;

/* Wall - a line segment forming a sector boundary */
typedef struct {
    int32_t  x, y;              /* Wall start point (world coords) */
    int16_t  point2;            /* Index of next wall in loop */
    int16_t  nextwall;          /* Index of wall on other side (-1 = solid) */
    int16_t  nextsector;        /* Index of sector on other side (-1 = solid) */
    int16_t  cstat;             /* Rendering flags */
    int16_t  picnum;            /* Tile number */
    int16_t  overpicnum;        /* Overlay tile (masked wall) */
    int8_t   shade;             /* Shade level */
    uint8_t  pal;               /* Palette swap */
    uint8_t  xrepeat;           /* Texture X repeat */
    uint8_t  yrepeat;           /* Texture Y repeat */
    uint8_t  xpanning;          /* Texture X offset */
    uint8_t  ypanning;          /* Texture Y offset */
    int16_t  lotag;             /* Low tag */
    int16_t  hitag;             /* High tag */
    int16_t  extra;             /* Extra data */
} walltype;

/* Sprite - an object in the world */
typedef struct {
    int32_t  x, y, z;           /* World position */
    int16_t  cstat;             /* Rendering flags */
    int16_t  picnum;            /* Tile number */
    int8_t   shade;             /* Shade level */
    uint8_t  pal;               /* Palette swap */
    uint8_t  clipdist;          /* Clipping distance */
    uint8_t  filler;            /* Padding */
    uint8_t  xrepeat;           /* X size */
    uint8_t  yrepeat;           /* Y size */
    int8_t   xoffset;           /* X center offset */
    int8_t   yoffset;           /* Y center offset */
    int16_t  sectnum;           /* Current sector */
    int16_t  statnum;           /* Status number (game use) */
    int16_t  ang;               /* Angle (0-2047) */
    int16_t  owner;             /* Owner sprite */
    int16_t  xvel, yvel, zvel;  /* Velocity components */
    int16_t  lotag;             /* Low tag */
    int16_t  hitag;             /* High tag */
    int16_t  extra;             /* Extra data */
} spritetype;

/* Tile size info */
typedef struct {
    int16_t x, y;
} vec2_16;

/* ============================================================
 * Sprite status list (for quick iteration by type)
 * ============================================================ */
typedef struct {
    int16_t headspritesect[MAXSECTORS + 1];
    int16_t headspritestat[MAXSTATUS + 1];
    int16_t prevspritesect[MAXSPRITES];
    int16_t prevspritestat[MAXSPRITES];
    int16_t nextspritesect[MAXSPRITES];
    int16_t nextspritestat[MAXSPRITES];
} spritelisttype;

/* ============================================================
 * Wall rendering flags (cstat)
 * ============================================================ */
#define WALLCSTAT_BLOCK      1     /* Blocks movement */
#define WALLCSTAT_BOTTOMSWAP 2     /* Bottom wall swapped */
#define WALLCSTAT_ALIGNBOT   4     /* Align texture to bottom */
#define WALLCSTAT_XFLIP      8     /* X-flip texture */
#define WALLCSTAT_MASKING     16    /* Masked/translucent */
#define WALLCSTAT_ONESIDED   32    /* One-sided wall */
#define WALLCSTAT_BLOCK_HIT  64    /* Blocks hitscan */
#define WALLCSTAT_TRANS       128   /* Translucent */
#define WALLCSTAT_YFLIP       256   /* Y-flip texture */
#define WALLCSTAT_TRANS_REV   512   /* Reverse translucent */

/* ============================================================
 * Sprite rendering flags (cstat)
 * ============================================================ */
#define SPRCSTAT_BLOCK       1     /* Blocks movement */
#define SPRCSTAT_TRANS       2     /* Translucent */
#define SPRCSTAT_XFLIP       4     /* X-flip */
#define SPRCSTAT_YFLIP       8     /* Y-flip */
#define SPRCSTAT_WALL        16    /* Wall sprite (flat) */
#define SPRCSTAT_FLOOR       32    /* Floor sprite (flat) */
#define SPRCSTAT_ONESIDED    64    /* One-sided */
#define SPRCSTAT_REALCENTER  128   /* Center sprite on actual center */
#define SPRCSTAT_BLOCKHIT    256   /* Blocks hitscan */
#define SPRCSTAT_TRANS_REV   512   /* Reverse translucent */
#define SPRCSTAT_INVISIBLE   0x8000 /* Invisible */

/* ============================================================
 * Sector rendering flags (ceilingstat/floorstat)
 * ============================================================ */
#define SECSTAT_PARALLAX     1     /* Parallax sky */
#define SECSTAT_SLOPE        2     /* Sloped */
#define SECSTAT_SWAPXY       4     /* Swap X/Y texture */
#define SECSTAT_EXPAND       8     /* Double texture size */
#define SECSTAT_XFLIP        16    /* X-flip texture */
#define SECSTAT_YFLIP        32    /* Y-flip texture */
#define SECSTAT_RELATIVE     64    /* Relative alignment */
#define SECSTAT_MASKING      128   /* Masking (for ceilings) */

/* ============================================================
 * Rendering state for sprites (for sorted drawing)
 * ============================================================ */
typedef struct {
    int32_t  sx, sy;     /* Screen position */
    int32_t  dist;       /* Distance from camera */
    int16_t  sprite_idx; /* Index into sprite array */
    int16_t  owner;      /* Owner (for sorting) */
} tsprtype;

/* ============================================================
 * Global engine state
 * ============================================================ */

/* Map data */
extern sectortype  sector[MAXSECTORS];
extern walltype    wall[MAXWALLS];
extern spritetype  sprite[MAXSPRITES];
extern int16_t     numsectors, numwalls;
extern int16_t     numsprites;

/* Sprite linked lists */
extern spritelisttype sprlist;

/* Tile/texture data */
extern int16_t     tilesizx[MAXTILES];
extern int16_t     tilesizy[MAXTILES];
extern uint32_t    picanm[MAXTILES];
extern uint8_t    *tileptr[MAXTILES];

/* Palette */
extern uint8_t     palette_raw[768];       /* Original 6-bit DAT */
extern uint32_t    palette32[256];         /* Converted ABGR8888 */
extern uint8_t     palookup[64][256];      /* Shade remap tables */
extern int32_t     numshades;

/* Trig tables */
extern int32_t     sintable[2048 + 512];   /* Extra for cos lookup */

/* Software framebuffer */
extern uint32_t    framebuffer[XDIM * YDIM];

/* ============================================================
 * Engine API functions
 * ============================================================ */

/* Initialization */
void  initengine(void);
void  uninitengine(void);

/* Rendering */
void  drawrooms(int32_t posx, int32_t posy, int32_t posz,
                int16_t ang, int16_t horiz, int16_t sectnum);
void  drawmasks(void);

/* Map operations */
int   loadboard(const char *filename, int32_t *posx, int32_t *posy,
                int32_t *posz, int16_t *ang, int16_t *sectnum);

/* Tile operations */
void  loadtile(int16_t tilenum);
uint8_t *get_tile_data(int16_t tilenum);

/* Palette */
int   loadpalette(const char *palfile, const char *lookupfile);

/* Sector queries */
int   updatesector(int32_t x, int32_t y, int16_t *sectnum);
int   inside(int32_t x, int32_t y, int16_t sectnum);

/* Physics helpers */
void  getzrange(int32_t x, int32_t y, int32_t z, int16_t sectnum,
                int32_t *ceilz, int32_t *florz, int32_t walldist);
int   clipmove(int32_t *x, int32_t *y, int32_t *z, int16_t *sectnum,
               int32_t xvect, int32_t yvect, int32_t walldist,
               int32_t ceildist, int32_t flordist);

/* Hitscan */
typedef struct {
    int16_t sect;
    int16_t wall;
    int16_t sprite;
    int32_t x, y, z;
} hitdata_t;

int   hitscan(int32_t xs, int32_t ys, int32_t zs, int16_t sectnum,
              int32_t vx, int32_t vy, int32_t vz, hitdata_t *hit);

/* Sprite management */
int   insertsprite(int16_t sectnum, int16_t statnum);
int   deletesprite(int16_t spritenum);
int   changespritesect(int16_t spritenum, int16_t newsectnum);
int   changespritestat(int16_t spritenum, int16_t newstatnum);

/* Utility */
int32_t ksqrt(int32_t val);
void  rotatepoint(int32_t xpivot, int32_t ypivot, int32_t x, int32_t y,
                  int16_t ang, int32_t *ox, int32_t *oy);
int32_t getangle(int32_t xvect, int32_t yvect);

#endif /* ENGINE_H */
