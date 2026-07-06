#ifndef ATA_H
#define ATA_H

#include <stdint.h>

int ata_read_sectors(uint16_t cylinder, uint8_t head, uint8_t sector, uint8_t count, void *buffer);
void lba_to_chs(uint32_t lba, uint16_t *cylinder, uint8_t *head, uint8_t *sector);

#endif