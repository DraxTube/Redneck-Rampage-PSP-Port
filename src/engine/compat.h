/*
 * compat.h - Compatibility layer and fixed-point math
 * Redneck Rampage PSP Port
 *
 * Provides platform-agnostic types, fixed-point math operations,
 * and Build engine math primitives adapted for MIPS.
 */

#ifndef COMPAT_H
#define COMPAT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================
 * Basic type aliases (Build engine conventions)
 * ============================================================ */
typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int8_t   SBYTE;
typedef int16_t  SWORD;
typedef int32_t  SDWORD;

/* ============================================================
 * Boolean
 * ============================================================ */
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* ============================================================
 * Build engine constants
 * ============================================================ */
#define MAXSECTORS   512
#define MAXWALLS     4096
#define MAXSPRITES   2048
#define MAXTILES     6144
#define MAXSTATUS    1024
#define MAXPLAYERS   1
#define MAXSPRITESONSCREEN 512

/* PSP rendering resolution */
#define XDIM         320
#define YDIM         200
#define HALFXDIM     (XDIM / 2)
#define HALFYDIM     (YDIM / 2)

/* PSP native display */
#define PSP_SCR_W    480
#define PSP_SCR_H    272
#define PSP_BUF_W    512   /* HW stride (power of 2) */

/* Build engine angle units: 2048 = 360 degrees */
#define BUILDANG_MASK 2047
#define BUILDANG_90   512
#define BUILDANG_180  1024
#define BUILDANG_270  1536

/* Fixed-point scale */
#define FP_SHIFT     16
#define FP_ONE       (1 << FP_SHIFT)

/* ============================================================
 * Fixed-point math (Build engine style)
 *
 * mulscale(a, b, shift) = (int32_t)((int64_t)a * b >> shift)
 * divscale(a, b, shift) = (int32_t)((int64_t)a << shift) / b
 * ============================================================ */
static inline int32_t mulscale(int32_t a, int32_t b, int32_t shift) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> shift);
}

static inline int32_t mulscale16(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

static inline int32_t mulscale26(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 26);
}

static inline int32_t mulscale30(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * (int64_t)b) >> 30);
}

static inline int32_t divscale(int32_t a, int32_t b, int32_t shift) {
    if (b == 0) return 0;
    return (int32_t)(((int64_t)a << shift) / (int64_t)b);
}

static inline int32_t divscale16(int32_t a, int32_t b) {
    if (b == 0) return 0;
    return (int32_t)(((int64_t)a << 16) / (int64_t)b);
}

static inline int32_t divscale26(int32_t a, int32_t b) {
    if (b == 0) return 0;
    return (int32_t)(((int64_t)a << 26) / (int64_t)b);
}

static inline int32_t dmulscale(int32_t a, int32_t b, int32_t c, int32_t d, int32_t shift) {
    return (int32_t)(((int64_t)a * b + (int64_t)c * d) >> shift);
}

static inline int32_t dmulscale16(int32_t a, int32_t b, int32_t c, int32_t d) {
    return (int32_t)(((int64_t)a * b + (int64_t)c * d) >> 16);
}

/* Absolute value */
static inline int32_t klabs(int32_t a) {
    return (a < 0) ? -a : a;
}

/* Sign of value */
static inline int32_t ksgn(int32_t a) {
    if (a > 0) return 1;
    if (a < 0) return -1;
    return 0;
}

/* Min/Max */
static inline int32_t kmin(int32_t a, int32_t b) {
    return (a < b) ? a : b;
}

static inline int32_t kmax(int32_t a, int32_t b) {
    return (a > b) ? a : b;
}

static inline int32_t kclamp(int32_t val, int32_t lo, int32_t hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/* ============================================================
 * Memory helpers
 * ============================================================ */
static inline void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p && size > 0) {
        /* On PSP, out of memory is fatal */
        return NULL;
    }
    return p;
}

static inline void *xcalloc(size_t count, size_t size) {
    void *p = calloc(count, size);
    return p;
}

static inline void xfree(void *p) {
    if (p) free(p);
}

/* ============================================================
 * Endian helpers (PSP is little-endian, Build data is LE too)
 * ============================================================ */
static inline int16_t read_le16(const uint8_t *p) {
    return (int16_t)(p[0] | (p[1] << 8));
}

static inline uint16_t read_le16u(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline int32_t read_le32(const uint8_t *p) {
    return (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static inline uint32_t read_le32u(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

#endif /* COMPAT_H */
