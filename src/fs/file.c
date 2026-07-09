#include "file.h"
#include "../misc/mem.h"
#include "../mm/heap.h"

open_file_t open_files[MAX_OPEN_FILES];

int file_open(const char *path, int writable) {
    fat_stat_t st;
    int exists = (fat_stat(path, &st) == 0);

    if (!writable && !exists)
        return -1;

    int slot = -1;
    
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].in_use) {
            slot = i;

            break;
        }
    }

    if (slot < 0)
        return -1;

    if (writable && fat_write_file(path, 0, 0))
        return -1;

    open_file_t *f = &open_files[slot];

    f->in_use = 1;
    f->offset = 0;
    f->writable = writable;

    int j = 0;

    while (path[j] && j < 63) {
        f->path[j] = path[j];
        j++;
    }

    f->path[j] = '\0';

    f->size = writable ? 0 : st.size;

    return slot;

    return -1;
}

void file_close(int idx) {
    if (idx < 0 || idx >= MAX_OPEN_FILES)
        return;

    open_files[idx].in_use = 0;
}

int file_read(int idx, void *buf, uint32_t len) {
    if (idx < 0 || idx >= MAX_OPEN_FILES || !open_files[idx].in_use)
        return -1;

    open_file_t *f = &open_files[idx];
    uint32_t alloc_size = fat_read_alloc_size(f->size);
    uint8_t *tmp = (uint8_t *)kmalloc(alloc_size);

    if (!tmp)
        return -1;

    uint32_t actual_size;

    if (fat_read_file(f->path, tmp, &actual_size)) {
        kfree((uint32_t)tmp);

        return -1;
    }

    uint32_t remaining = f->size - f->offset;
    uint32_t to_copy = len < remaining ? len : remaining;

    memcpy(buf, tmp + f->offset, to_copy);
    kfree((uint32_t)tmp);

    f->offset += to_copy;

    return (int)to_copy;
}

int file_write(int idx, const void *buf, uint32_t len) {
    if (idx < 0 || idx >= MAX_OPEN_FILES || !open_files[idx].in_use || !open_files[idx].writable)
        return -1;

    open_file_t *f = &open_files[idx];

    if (fat_write_at(f->path, f->offset, buf, len))
        return -1;

    f->offset += len;

    if (f->offset > f->size)
        f->size = f->offset;

    return (int)len;
}