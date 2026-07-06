#ifndef FAT_H
#define FAT_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t jmp[3];
    uint8_t oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} fat12_bpb_t;

typedef struct __attribute__((packed)) {
    uint8_t name[8];
    uint8_t ext[3];
    uint8_t attr;
    uint8_t reserved;
    uint8_t create_time_tenths;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat12_dirent_t;

typedef struct {
    char name[13];
    uint32_t size;
    uint16_t first_cluster;
    uint8_t attr;
} fat_stat_t;

typedef struct {
    char name[13];
    uint32_t size;
    uint16_t first_cluster;
    uint8_t attr;
} fat_dirent_info_t;

int fat_init();
int fat_read_file(const char *path, void *dest, uint32_t *size_out);
int fat_write_file(const char *path, const void *data, uint32_t size);
int fat_write_at(const char *path, uint32_t offset, const void *data, uint32_t len);
int fat_delete_file(const char *path);
int fat_stat(const char *path, fat_stat_t *out);
int fat_mkdir(const char *path);
int fat_rmdir(const char *path);
int fat_opendir(const char *path, uint16_t *dir_cluster_out);
int fat_readdir(uint16_t dir_cluster, uint32_t index, fat_dirent_info_t *out);

#endif