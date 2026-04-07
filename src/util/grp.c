/*
 * grp.c - GRP archive reader implementation
 * Redneck Rampage PSP Port
 */

#include "grp.h"
#include <ctype.h>

grp_file_t *g_grp = NULL;

/* Case-insensitive string compare for filenames */
static int grp_strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 1;
        a++;
        b++;
    }
    return (*a != *b) ? 1 : 0;
}

grp_file_t *grp_open(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return NULL;
    }

    /* Read and verify signature */
    char sig[GRP_SIG_LEN];
    if (fread(sig, 1, GRP_SIG_LEN, fp) != GRP_SIG_LEN) {
        fclose(fp);
        return NULL;
    }

    if (memcmp(sig, GRP_SIG, GRP_SIG_LEN) != 0) {
        fclose(fp);
        return NULL;
    }

    /* Read number of files */
    uint8_t buf4[4];
    if (fread(buf4, 1, 4, fp) != 4) {
        fclose(fp);
        return NULL;
    }
    int32_t num_files = read_le32(buf4);

    if (num_files < 0 || num_files > GRP_MAX_FILES) {
        fclose(fp);
        return NULL;
    }

    /* Allocate GRP handle */
    grp_file_t *grp = (grp_file_t *)xcalloc(1, sizeof(grp_file_t));
    if (!grp) {
        fclose(fp);
        return NULL;
    }

    grp->fp = fp;
    grp->num_files = num_files;
    grp->entries = (grp_entry_t *)xcalloc(num_files, sizeof(grp_entry_t));
    if (!grp->entries) {
        fclose(fp);
        free(grp);
        return NULL;
    }

    /* Read directory entries */
    int32_t data_offset = GRP_SIG_LEN + 4 + (num_files * 16);
    int32_t current_offset = 0;

    for (int i = 0; i < num_files; i++) {
        uint8_t entry_buf[16];
        if (fread(entry_buf, 1, 16, fp) != 16) {
            grp_close(grp);
            return NULL;
        }

        /* Copy filename (12 bytes, may not be null-terminated) */
        memcpy(grp->entries[i].name, entry_buf, GRP_NAME_LEN);
        grp->entries[i].name[GRP_NAME_LEN] = '\0';

        /* Trim trailing spaces */
        for (int j = GRP_NAME_LEN - 1; j >= 0; j--) {
            if (grp->entries[i].name[j] == ' ' || grp->entries[i].name[j] == '\0')
                grp->entries[i].name[j] = '\0';
            else
                break;
        }

        /* File size (4 bytes LE) */
        grp->entries[i].size = read_le32(entry_buf + 12);
        grp->entries[i].offset = current_offset;
        current_offset += grp->entries[i].size;
    }

    grp->data_offset = data_offset;

    return grp;
}

void grp_close(grp_file_t *grp) {
    if (!grp) return;
    if (grp->fp) fclose(grp->fp);
    if (grp->entries) free(grp->entries);
    free(grp);
}

int grp_find(grp_file_t *grp, const char *name) {
    if (!grp || !name) return -1;

    for (int i = 0; i < grp->num_files; i++) {
        if (grp_strcasecmp(grp->entries[i].name, name) == 0)
            return i;
    }
    return -1;
}

uint8_t *grp_read(grp_file_t *grp, int index, int32_t *out_size) {
    if (!grp || index < 0 || index >= grp->num_files)
        return NULL;

    grp_entry_t *e = &grp->entries[index];

    /* Seek to file data */
    int32_t abs_offset = grp->data_offset + e->offset;
    if (fseek(grp->fp, abs_offset, SEEK_SET) != 0)
        return NULL;

    /* Allocate and read */
    uint8_t *data = (uint8_t *)xmalloc(e->size);
    if (!data) return NULL;

    if ((int32_t)fread(data, 1, e->size, grp->fp) != e->size) {
        free(data);
        return NULL;
    }

    if (out_size) *out_size = e->size;
    return data;
}

uint8_t *grp_read_file(grp_file_t *grp, const char *name, int32_t *out_size) {
    int idx = grp_find(grp, name);
    if (idx < 0) return NULL;
    return grp_read(grp, idx, out_size);
}

const grp_entry_t *grp_get_entry(grp_file_t *grp, int index) {
    if (!grp || index < 0 || index >= grp->num_files)
        return NULL;
    return &grp->entries[index];
}
