#include "fat.h"
#include "../devices/ata.h"
#include "../misc/util.h"
#include "../misc/mem.h"
#include "../mm/heap.h"

#define SECTOR_SIZE 512
#define DIRENT_SIZE 32
#define ATTR_LONG_NAME 0x0f
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define FAT12_EOC 0x0ff8
#define FAT12_FREE 0x000
#define FAT12_EOC_MARK 0x0fff

typedef struct {
    uint32_t sector;
    uint32_t offset;
    int exists;
} dir_slot_t;

static fat12_bpb_t bpb;
static uint32_t fat_start;
static uint32_t root_start;
static uint32_t root_sectors;
static uint32_t data_start;
static uint8_t sector_buf[SECTOR_SIZE];
static uint8_t fat_buf[SECTOR_SIZE * 9];
static uint8_t *cluster_buf;
static uint32_t cluster_size;

static int read_lba(uint32_t lba, uint8_t count, void *buf) {
    uint16_t cyl;
    uint8_t head, sect;
    uint8_t *p = (uint8_t *)buf;

    for (uint8_t i = 0; i < count; i++) {
        lba_to_chs(lba + i, &cyl, &head, &sect);

        if (ata_read_sectors(cyl, head, sect, 1, p))
            return 1;

        p += SECTOR_SIZE;
    }

    return 0;
}

static int write_lba(uint32_t lba, uint8_t count, const void *buf) {
    uint16_t cyl;
    uint8_t head, sect;
    const uint8_t *p = (const uint8_t *)buf;

    for (uint8_t i = 0; i < count; i++) {
        lba_to_chs(lba + i, &cyl, &head, &sect);
        
        if (ata_write_sectors(cyl, head, sect, 1, p))
            return 1;

        p += SECTOR_SIZE;
    }

    return 0;
}

static uint16_t fat_next(uint16_t cluster) {
    uint32_t off = cluster + (cluster / 2);
    uint16_t val = fat_buf[off] | (fat_buf[off + 1] << 8);

    return (cluster & 1) ? (val >> 4) : (val & 0x0FFF);
}

static void fat_set_entry(uint16_t cluster, uint16_t value) {
    uint32_t off = cluster + (cluster / 2);

    value &= 0x0fff;

    if (cluster & 1) {
        fat_buf[off] = (fat_buf[off] & 0x0f) | ((value & 0x0f) << 4);
        fat_buf[off + 1] = (value >> 4) & 0xff;
    } else {
        fat_buf[off] = value & 0xff;
        fat_buf[off + 1] = (fat_buf[off + 1] & 0xf0) | ((value >> 8) & 0x0f);
    }
}

static void to_fat_name(const char *name, uint8_t out[11]) {
    memset(out, ' ', 11);

    int i = 0;

    for (; name[i] && name[i] != '.' && i < 8; i++)
        out[i] = to_upper(name[i]);

    if (name[i] == '.') {
        i++;

        for (int j = 0; name[i] && j < 3; i++, j++)
            out[8 + j] = to_upper(name[i]);
    }
}

static void from_fat_name(const uint8_t raw[11], char out[13]) {
    int k = 0;

    for (int i = 0; i < 8 && raw[i] != ' '; i++)
        out[k++] = raw[i];

    if (raw[8] != ' ') {
        out[k++] = '.';

        for (int i = 8; i < 11 && raw[i] != ' '; i++)
            out[k++] = raw[i];
    }

    out[k] = '\0';
}

static uint32_t fat_total_clusters() {
    uint32_t total_sectors = bpb.total_sectors_16 ? bpb.total_sectors_16 : bpb.total_sectors_32;
    uint32_t data_sectors = total_sectors - data_start;

    return data_sectors / bpb.sectors_per_cluster + 2;
}

static uint16_t fat_alloc_cluster() {
    uint32_t max = fat_total_clusters();

    for (uint16_t c = 2; c < max; c++) {
        if (fat_next(c) == FAT12_FREE) {
            fat_set_entry(c, FAT12_EOC_MARK);
            
            return c;
        }
    }

    return 0;
}

static void fat_free_chain(uint16_t cluster) {
    while (cluster >= 2 && cluster < FAT12_EOC) {
        uint16_t next = fat_next(cluster);

        fat_set_entry(cluster, FAT12_FREE);

        cluster = next;
    }
}

static int fat_flush() {
    for (uint8_t i = 0; i < bpb.fat_count; i++) {
        uint32_t lba = fat_start + (uint32_t)i * bpb.sectors_per_fat;

        if (write_lba(lba, bpb.sectors_per_fat, fat_buf))
            return 1;
    }

    return 0;
}

static int split_next(const char **pp, char comp[13]) {
    const char *p = *pp;

    while (*p == '/')
        p++;

    if (*p == '\0')
        return 0;

    int i = 0;

    while (*p && *p != '/' && i < 12)
        comp[i++] = *p++;

    comp[i] = '\0';

    *pp = p;

    return 1;
}

static int dir_lookup(uint16_t dir_cluster, const uint8_t target[11], dir_slot_t *slot, int create) {
    int have_free = 0;
    dir_slot_t free_slot = {0};

    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < root_sectors; s++) {
            uint32_t lba = root_start + s;

            if (read_lba(lba, 1, sector_buf))
                return 1;

            fat12_dirent_t *e = (fat12_dirent_t *)sector_buf;

            for (uint32_t i = 0; i < SECTOR_SIZE / DIRENT_SIZE; i++, e++) {
                if (e->name[0] == 0x00) {
                    if (!have_free) {
                        free_slot.sector = lba;
                        free_slot.offset = i * DIRENT_SIZE;
                        have_free = 1;
                    }

                    *slot = free_slot;
                    slot->exists = 0;

                    return 0;
                }

                if (e->name[0] == 0xe5) {
                    if (!have_free) {
                        free_slot.sector = lba;
                        free_slot.offset = i * DIRENT_SIZE;
                        have_free = 1;
                    }

                    continue;
                }

                if (e->attr == ATTR_LONG_NAME)
                    continue;

                if (e->attr & ATTR_VOLUME_ID)
                    continue;

                if (memcmp(e->name, target, 11) == 0) {
                    slot->sector = lba;
                    slot->offset = i * DIRENT_SIZE;
                    slot->exists = 1;

                    return 0;
                }
            }
        }

        if (have_free) {
            *slot = free_slot;
            slot->exists = 0;

            return 0;
        }

        return 2;
    }

    uint16_t cluster = dir_cluster;

    while (1) {
        for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++) {
            uint32_t lba = data_start + (uint32_t)(cluster - 2) * bpb.sectors_per_cluster + s;

            if (read_lba(lba, 1, sector_buf))
                return 1;

            fat12_dirent_t *e = (fat12_dirent_t *)sector_buf;

            for (uint32_t i = 0; i < SECTOR_SIZE / DIRENT_SIZE; i++, e++) {
                if (e->name[0] == 0x00) {
                    if (!have_free) {
                        free_slot.sector = lba;
                        free_slot.offset = i * DIRENT_SIZE;
                        have_free = 1;
                    }

                    *slot = free_slot;
                    slot->exists = 0;

                    return 0;
                }

                if (e->name[0] == 0xe5) {
                    if (!have_free) {
                        free_slot.sector = lba;
                        free_slot.offset = i * DIRENT_SIZE;
                        have_free = 1;
                    }

                    continue;
                }
                
                if (e->attr == ATTR_LONG_NAME)
                    continue;

                if (e->attr & ATTR_VOLUME_ID)
                    continue;

                if (memcmp(e->name, target, 11) == 0) {
                    slot->sector = lba;
                    slot->offset = i * DIRENT_SIZE;
                    slot->exists = 1;

                    return 0;
                }
            }
        }

        uint16_t next = fat_next(cluster);

        if (next >= 2 && next < FAT12_EOC) {
            cluster = next;

            continue;
        }

        if (have_free) {
            *slot = free_slot;
            slot->exists = 0;

            return 0;
        }

        if (!create)
            return 2;

        uint16_t newc = fat_alloc_cluster();

        if (!newc)
            return 3;

        fat_set_entry(cluster, newc);

        memset(cluster_buf, 0, cluster_size);

        uint32_t lba0 = data_start + (uint32_t)(newc - 2) * bpb.sectors_per_cluster;

        if (write_lba(lba0, bpb.sectors_per_cluster, cluster_buf))
            return 1;

        if (fat_flush())
            return 1;

        slot->sector = lba0;
        slot->offset = 0;
        slot->exists = 0;

        return 0;
    }
}

static int dir_is_empty(uint16_t dir_cluster) {
    uint16_t cluster = dir_cluster;

    while (cluster >= 2 && cluster < FAT12_EOC) {
        for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++) {
            uint32_t lba = data_start + (uint32_t)(cluster - 2) * bpb.sectors_per_cluster + s;

            if (read_lba(lba, 1, sector_buf))
                return -1;

            fat12_dirent_t *e = (fat12_dirent_t *)sector_buf;

            for (uint32_t i = 0; i < SECTOR_SIZE / DIRENT_SIZE; i++, e++) {
                if (e->name[0] == 0x00)
                    continue;

                if (e->name[0] == 0xe5)
                    continue;

                if (e->attr == ATTR_LONG_NAME)
                    continue;

                if (e->name[0] == '.' && e->name[1] == ' ')
                    continue;

                if (e->name[0] == '.' && e->name[1] == '.' && e->name[2] == ' ')
                    continue;

                return 0;
            }
        }

        cluster = fat_next(cluster);
    }

    return 1;
}

static int resolve_path(const char *path, uint16_t *dir_cluster_out, uint8_t final_name[11]) {
    uint16_t cur = 0;
    const char *p = path;
    char comp[13];
    char pending[13] = {0};
    int have_pending = 0;

    while (split_next(&p, comp)) {
        if (have_pending) {
            uint8_t t[11];

            to_fat_name(pending, t);

            dir_slot_t slot;
            int r = dir_lookup(cur, t, &slot, 0);

            if (r == 1)
                return 1;

            if (r == 2 || !slot.exists)
                return 2;

            if (read_lba(slot.sector, 1, sector_buf))
                return 1;

            fat12_dirent_t *e = (fat12_dirent_t *)(sector_buf + slot.offset);

            if (!(e->attr & ATTR_DIRECTORY))
                return 3;

            cur = e->first_cluster_low;
        }

        memcpy(pending, comp, sizeof(pending));
        
        have_pending = 1;
    }

    if (!have_pending)
        return 4;

    to_fat_name(pending, final_name);

    *dir_cluster_out = cur;

    return 0;
}

int fat_init() {
    if (read_lba(0, 1, sector_buf))
        return 1;

    memcpy(&bpb, sector_buf, sizeof(bpb));

    if (bpb.bytes_per_sector != SECTOR_SIZE)
        return 2;

    fat_start = bpb.reserved_sectors;
    root_start = fat_start + (uint32_t)bpb.fat_count * bpb.sectors_per_fat;
    root_sectors = (bpb.root_entry_count * DIRENT_SIZE + SECTOR_SIZE - 1) / SECTOR_SIZE;
    data_start = root_start + root_sectors;

    if (bpb.sectors_per_fat * SECTOR_SIZE > sizeof(fat_buf))
        return 3;
    
    cluster_size = (uint32_t)bpb.sectors_per_cluster * SECTOR_SIZE;
    cluster_buf = (uint8_t *)kmalloc(cluster_size);

    if (!cluster_buf)
        return 5;

    return read_lba(fat_start, bpb.sectors_per_fat, fat_buf) ? 4 : 0;
}

int fat_read_file(const char *path, void *dest, uint32_t *size_out) {
    uint16_t dir_cluster;
    uint8_t target[11];

    if (resolve_path(path, &dir_cluster, target))
        return 1;

    dir_slot_t slot;
    int r = dir_lookup(dir_cluster, target, &slot, 0);

    if (r != 0 || !slot.exists)
        return 1;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    fat12_dirent_t ent;

    memcpy(&ent, sector_buf + slot.offset, sizeof(ent));

    if (size_out)
        *size_out = ent.file_size;

    uint8_t *out = (uint8_t *)dest;
    uint16_t cluster = ent.first_cluster_low;

    while (cluster < FAT12_EOC && cluster >= 2) {
        uint32_t lba = data_start + (uint32_t)(cluster - 2) * bpb.sectors_per_cluster;

        if (read_lba(lba, bpb.sectors_per_cluster, out))
            return 2;

        out += (uint32_t)bpb.sectors_per_cluster * SECTOR_SIZE;
        cluster = fat_next(cluster);
    }

    return 0;
}

int fat_write_file(const char *path, const void *data, uint32_t size) {
    uint16_t dir_cluster;
    uint8_t target[11];

    if (resolve_path(path, &dir_cluster, target))
        return 1;

    dir_slot_t slot;
    int r = dir_lookup(dir_cluster, target, &slot, 1);

    if (r == 1)
        return 1;

    if (r == 2)
        return 2;

    if (slot.exists) {
        if (read_lba(slot.sector, 1, sector_buf))
            return 1;

        fat12_dirent_t *e = (fat12_dirent_t *)(sector_buf + slot.offset);

        if (e->first_cluster_low >= 2)
            fat_free_chain(e->first_cluster_low);
    }

    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;
    const uint8_t *src = (const uint8_t *)data;
    uint32_t remaining = size;

    while (remaining > 0) {
        uint16_t c = fat_alloc_cluster();

        if (c == 0) {
            if (first_cluster) fat_free_chain(first_cluster);

            return 3;
        }

        if (prev_cluster)
            fat_set_entry(prev_cluster, c);
        else
            first_cluster = c;

        uint32_t chunk = remaining < cluster_size ? remaining : cluster_size;

        memset(cluster_buf, 0, cluster_size);
        memcpy(cluster_buf, src, chunk);

        uint32_t lba = data_start + (uint32_t)(c - 2) * bpb.sectors_per_cluster;

        if (write_lba(lba, bpb.sectors_per_cluster, cluster_buf))
            return 1;

        src += chunk;
        remaining -= chunk;
        prev_cluster = c;
    }

    if (prev_cluster)
        fat_set_entry(prev_cluster, FAT12_EOC_MARK);

    if (fat_flush())
        return 1;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    fat12_dirent_t *e = (fat12_dirent_t *)(sector_buf + slot.offset);

    memset(e, 0, sizeof(*e));
    memcpy(e->name, target, 11);

    e->attr = 0x20;
    e->first_cluster_low = first_cluster;
    e->file_size = size;

    return write_lba(slot.sector, 1, sector_buf);
}

int fat_write_at(const char *path, uint32_t offset, const void *data, uint32_t len) {
    if (len == 0)
        return 0;

    uint16_t dir_cluster;
    uint8_t target[11];

    if (resolve_path(path, &dir_cluster, target))
        return 1;

    dir_slot_t slot;
    int r = dir_lookup(dir_cluster, target, &slot, 1);

    if (r == 1)
        return 1;

    if (r == 2)
        return 2;

    fat12_dirent_t ent;
    uint16_t first_cluster;
    uint32_t old_size;

    if (slot.exists) {
        if (read_lba(slot.sector, 1, sector_buf))
            return 1;

        memcpy(&ent, sector_buf + slot.offset, sizeof(ent));

        first_cluster = ent.first_cluster_low;
        old_size = ent.file_size;
    } else {
        memset(&ent, 0, sizeof(ent));
        memcpy(ent.name, target, 11);

        ent.attr = 0x20;
        first_cluster = 0;
        old_size = 0;
    }

    uint32_t start_idx = offset / cluster_size;
    uint32_t end_idx = (offset + len - 1) / cluster_size;
    uint16_t cluster = first_cluster;

    if (cluster < 2) {
        cluster = fat_alloc_cluster();

        if (!cluster)
            return 3;

        first_cluster = cluster;
    }

    for (uint32_t i = 0; i < start_idx; i++) {
        uint16_t next = fat_next(cluster);

        if (next == FAT12_FREE || next >= FAT12_EOC) {
            next = fat_alloc_cluster();
            
            if (!next)
                return 3;

            fat_set_entry(cluster, next);
        }

        cluster = next;
    }

    const uint8_t *src = (const uint8_t *)data;
    uint32_t bytes_left = len;
    uint32_t cur_off = offset % cluster_size;

    for (uint32_t idx = start_idx; idx <= end_idx; idx++) {
        uint32_t lba = data_start + (uint32_t)(cluster - 2) * bpb.sectors_per_cluster;
        uint32_t space = cluster_size - cur_off;
        uint32_t chunk = bytes_left < space ? bytes_left : space;
        int partial = (cur_off != 0) || (chunk < cluster_size);

        if (partial) {
            if (read_lba(lba, bpb.sectors_per_cluster, cluster_buf))
                return 1;
        }

        memcpy(cluster_buf + cur_off, src, chunk);

        if (write_lba(lba, bpb.sectors_per_cluster, cluster_buf))
            return 1;

        src += chunk;
        bytes_left -= chunk;
        cur_off = 0;

        if (idx < end_idx) {
            uint16_t next = fat_next(cluster);

            if (next == FAT12_FREE || next >= FAT12_EOC) {
                next = fat_alloc_cluster();

                if (!next)
                    return 3;

                fat_set_entry(cluster, next);
            }

            cluster = next;
        }
    }

    uint32_t new_size = offset + len;

    if (new_size < old_size)
        new_size = old_size;

    ent.first_cluster_low = first_cluster;
    ent.file_size = new_size;

    if (fat_flush())
        return 1;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    memcpy(sector_buf + slot.offset, &ent, sizeof(ent));

    return write_lba(slot.sector, 1, sector_buf);
}

int fat_delete_file(const char *path) {
    uint16_t dir_cluster;
    uint8_t target[11];

    if (resolve_path(path, &dir_cluster, target))
        return 1;

    dir_slot_t slot;
    int r = dir_lookup(dir_cluster, target, &slot, 0);

    if (r == 1)
        return 1;

    if (r == 2 || !slot.exists)
        return 2;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    fat12_dirent_t *e = (fat12_dirent_t *)(sector_buf + slot.offset);

    if (e->attr & ATTR_DIRECTORY)
        return 3;

    if (e->first_cluster_low >= 2)
        fat_free_chain(e->first_cluster_low);

    e->name[0] = 0xe5;

    if (write_lba(slot.sector, 1, sector_buf))
        return 1;

    return fat_flush();
}

int fat_stat(const char *path, fat_stat_t *out) {
    uint16_t dir_cluster;
    uint8_t target[11];

    if (resolve_path(path, &dir_cluster, target))
        return 1;

    dir_slot_t slot;
    int r = dir_lookup(dir_cluster, target, &slot, 0);

    if (r != 0 || !slot.exists)
        return 1;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    fat12_dirent_t ent;

    memcpy(&ent, sector_buf + slot.offset, sizeof(ent));
    from_fat_name(ent.name, out->name);

    out->size = ent.file_size;
    out->first_cluster = ent.first_cluster_low;
    out->attr = ent.attr;

    return 0;
}

int fat_mkdir(const char *path) {
    uint16_t parent_cluster;
    uint8_t target[11];

    if (resolve_path(path, &parent_cluster, target))
        return 1;

    dir_slot_t slot;
    int r = dir_lookup(parent_cluster, target, &slot, 1);

    if (r == 1)
        return 1;

    if (r == 2 || r == 3)
        return 2;

    if (slot.exists)
        return 3;

    uint16_t newc = fat_alloc_cluster();

    if (!newc)
        return 4;

    fat_set_entry(newc, FAT12_EOC_MARK);
    memset(cluster_buf, 0, cluster_size);

    fat12_dirent_t *entries = (fat12_dirent_t *)cluster_buf;

    memset(entries[0].name, ' ', 11);

    entries[0].name[0] = '.';
    entries[0].attr = ATTR_DIRECTORY;
    entries[0].first_cluster_low = newc;

    memset(entries[1].name, ' ', 11);

    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    entries[1].attr = ATTR_DIRECTORY;
    entries[1].first_cluster_low = parent_cluster;

    uint32_t lba = data_start + (uint32_t)(newc - 2) * bpb.sectors_per_cluster;

    if (write_lba(lba, bpb.sectors_per_cluster, cluster_buf))
        return 1;

    if (fat_flush())
        return 1;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    fat12_dirent_t *e = (fat12_dirent_t *)(sector_buf + slot.offset);

    memset(e, 0, sizeof(*e));
    memcpy(e->name, target, 11);

    e->attr = ATTR_DIRECTORY;
    e->first_cluster_low = newc;
    e->file_size = 0;

    return write_lba(slot.sector, 1, sector_buf);
}

int fat_rmdir(const char *path) {
    uint16_t parent_cluster;
    uint8_t target[11];

    if (resolve_path(path, &parent_cluster, target))
        return 1;

    dir_slot_t slot;
    int r = dir_lookup(parent_cluster, target, &slot, 0);

    if (r == 1)
        return 1;

    if (r == 2 || r == 3 || !slot.exists)
        return 2;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    fat12_dirent_t ent;

    memcpy(&ent, sector_buf + slot.offset, sizeof(ent));

    if (!(ent.attr & ATTR_DIRECTORY))
        return 3;

    uint16_t target_cluster = ent.first_cluster_low;

    if (target_cluster < 2)
        return 4;

    int empty = dir_is_empty(target_cluster);

    if (empty < 0)
        return 1;

    if (empty == 0)
        return 5;

    fat_free_chain(target_cluster);

    if (fat_flush())
        return 1;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    fat12_dirent_t *e = (fat12_dirent_t *)(sector_buf + slot.offset);

    e->name[0] = 0xe5;

    return write_lba(slot.sector, 1, sector_buf);
}

int fat_opendir(const char *path, uint16_t *dir_cluster_out) {
    const char *p = path;

    while (*p == '/')
        p++;

    if (*p == '\0') {
        *dir_cluster_out = 0;

        return 0;
    }

    uint16_t parent_cluster;
    uint8_t target[11];

    if (resolve_path(path, &parent_cluster, target))
        return 1;

    dir_slot_t slot;
    int r = dir_lookup(parent_cluster, target, &slot, 0);

    if (r == 1)
        return 1;

    if (r == 2 || r == 3 || !slot.exists)
        return 2;

    if (read_lba(slot.sector, 1, sector_buf))
        return 1;

    fat12_dirent_t ent;

    memcpy(&ent, sector_buf + slot.offset, sizeof(ent));

    if (!(ent.attr & ATTR_DIRECTORY))
        return 3;
        

    *dir_cluster_out = ent.first_cluster_low;
    
    return 0;
}

int fat_readdir(uint16_t dir_cluster, uint32_t index, fat_dirent_info_t *out) {
    uint32_t seen = 0;

    if (dir_cluster == 0) {
        for (uint32_t s = 0; s < root_sectors; s++) {
            if (read_lba(root_start + s, 1, sector_buf))
                return 1;

            fat12_dirent_t *e = (fat12_dirent_t *)sector_buf;

            for (uint32_t i = 0; i < SECTOR_SIZE / DIRENT_SIZE; i++, e++) {
                if (e->name[0] == 0x00)
                    continue;

                if (e->name[0] == 0xe5)
                    continue;

                if (e->attr == ATTR_LONG_NAME)
                    continue;

                if (e->attr & ATTR_VOLUME_ID)
                    continue;

                if (seen == index) {
                    from_fat_name(e->name, out->name);

                    out->size = e->file_size;
                    out->first_cluster = e->first_cluster_low;
                    out->attr = e->attr;

                    return 0;
                }

                seen++;
            }
        }

        return 2;
    }

    uint16_t cluster = dir_cluster;

    while (cluster >= 2 && cluster < FAT12_EOC) {
        for (uint32_t s = 0; s < bpb.sectors_per_cluster; s++) {
            uint32_t lba = data_start + (uint32_t)(cluster - 2) * bpb.sectors_per_cluster + s;

            if (read_lba(lba, 1, sector_buf))
                return 1;

            fat12_dirent_t *e = (fat12_dirent_t *)sector_buf;

            for (uint32_t i = 0; i < SECTOR_SIZE / DIRENT_SIZE; i++, e++) {
                if (e->name[0] == 0x00)
                    continue;

                if (e->name[0] == 0xe5)
                    continue;

                if (e->attr == ATTR_LONG_NAME)
                    continue;

                if (e->attr & ATTR_VOLUME_ID)
                    continue;

                if (e->name[0] == '.' && e->name[1] == ' ')
                    continue;

                if (e->name[0] == '.' && e->name[1] == '.' && e->name[2] == ' ')
                    continue;

                if (seen == index) {
                    from_fat_name(e->name, out->name);

                    out->size = e->file_size;
                    out->first_cluster = e->first_cluster_low;
                    out->attr = e->attr;

                    return 0;
                }

                seen++;
            }
        }

        cluster = fat_next(cluster);
    }

    return 2;
}

uint32_t fat_read_alloc_size(uint32_t file_size) {
    uint32_t cluster_size = (uint32_t)bpb.sectors_per_cluster * SECTOR_SIZE;
    
    return (file_size + cluster_size - 1) & ~(cluster_size - 1);
}