/*
 * grp.h - GRP (Group) archive file format
 * Redneck Rampage PSP Port
 *
 * GRP is Ken Silverman's archive format used by Build engine games.
 * Format: "KenSilverman" (12 bytes) + numfiles (4 bytes)
 *         + directory (numfiles × 16 bytes: name[12] + size[4])
 *         + concatenated file data
 */

#ifndef GRP_H
#define GRP_H

#include "compat.h"

#define GRP_SIG         "KenSilverman"
#define GRP_SIG_LEN     12
#define GRP_NAME_LEN    12
#define GRP_MAX_FILES   8192

typedef struct {
    char     name[GRP_NAME_LEN + 1]; /* Null-terminated filename */
    int32_t  size;                   /* File size in bytes */
    int32_t  offset;                 /* Offset from start of GRP data */
} grp_entry_t;

typedef struct {
    FILE        *fp;             /* File handle to GRP */
    int32_t      num_files;      /* Number of files in archive */
    grp_entry_t *entries;        /* File directory */
    int32_t      data_offset;    /* Start of file data in GRP */
} grp_file_t;

/* Open a GRP archive file */
grp_file_t *grp_open(const char *filename);

/* Close GRP archive and free resources */
void grp_close(grp_file_t *grp);

/* Find a file in the GRP archive (case-insensitive) */
int grp_find(grp_file_t *grp, const char *name);

/* Read a file from the GRP archive into a new buffer */
uint8_t *grp_read(grp_file_t *grp, int index, int32_t *out_size);

/* Read a file by name from the GRP archive */
uint8_t *grp_read_file(grp_file_t *grp, const char *name, int32_t *out_size);

/* Get file entry info */
const grp_entry_t *grp_get_entry(grp_file_t *grp, int index);

/* Global GRP handle */
extern grp_file_t *g_grp;

#endif /* GRP_H */
