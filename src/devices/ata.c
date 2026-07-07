#include "ata.h"
#include "../misc/util.h"

#define ATA_DATA 0x1f0
#define ATA_ERROR 0x1f1
#define ATA_SECCOUNT 0x1f2
#define ATA_SECTOR 0x1f3
#define ATA_CYL_LOW 0x1f4
#define ATA_CYL_HIGH 0x1f5
#define ATA_DRIVE_HEAD 0x1f6
#define ATA_COMMAND 0x1f7
#define ATA_STATUS 0x1f7

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_CACHE_FLUSH 0xe7

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

#define ATA_CONTROL 0x3f6

#define SECTORS_PER_TRACK 63
#define HEADS_PER_CYLINDER 16

static void ata_wait_bsy() {
    while (inb(ATA_STATUS) & ATA_SR_BSY);
}

static void ata_wait_drq() {
    while (!(inb(ATA_STATUS) & ATA_SR_DRQ));
}

static inline void ata_400ns_delay() {
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
    inb(ATA_CONTROL);
}

int ata_read_sectors(uint16_t cylinder, uint8_t head, uint8_t sector, uint8_t count, void *buffer) {
    uint16_t *buf = (uint16_t *)buffer;

    ata_wait_bsy();

    outb(ATA_DRIVE_HEAD, 0xa0 | (head & 0x0f));

    ata_400ns_delay();
    ata_wait_bsy(); 

    outb(ATA_SECCOUNT, count);
    outb(ATA_SECTOR, sector);
    outb(ATA_CYL_LOW, (uint8_t)(cylinder & 0xff));
    outb(ATA_CYL_HIGH, (uint8_t)((cylinder >> 8) & 0xff));
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    ata_400ns_delay();

    for (uint8_t s = 0; s < count; s++) {
        ata_wait_bsy();

        uint8_t status = inb(ATA_STATUS);

        if (status & ATA_SR_ERR)
            return 1;

        ata_wait_drq();
        
        for (int i = 0; i < 256; i++)
            buf[i] = inw(ATA_DATA);
        
        buf += 256;
    }

    return 0;
}

int ata_write_sectors(uint16_t cylinder, uint8_t head, uint8_t sector, uint8_t count, const void *buffer) {
    const uint16_t *buf = (const uint16_t *)buffer;

    ata_wait_bsy();

    outb(ATA_DRIVE_HEAD, 0xa0 | (head & 0x0f));

    ata_400ns_delay();
    ata_wait_bsy();

    outb(ATA_SECCOUNT, count);
    outb(ATA_SECTOR, sector);
    outb(ATA_CYL_LOW, (uint8_t)(cylinder & 0xff));
    outb(ATA_CYL_HIGH, (uint8_t)((cylinder >> 8) & 0xff));
    outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);

    ata_400ns_delay();

    for (uint8_t s = 0; s < count; s++) {
        ata_wait_bsy();

        uint8_t status = inb(ATA_STATUS);

        if (status & ATA_SR_ERR)
            return 1;

        ata_wait_drq();

        for (int i = 0; i < 256; i++)
            outw(ATA_DATA, buf[i]);

        buf += 256;
    }

    outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
    
    ata_wait_bsy();

    return 0;
}

void lba_to_chs(uint32_t lba, uint16_t *cylinder, uint8_t *head, uint8_t *sector) {
    *sector = (lba % SECTORS_PER_TRACK) + 1;
    *head = (lba / SECTORS_PER_TRACK) % HEADS_PER_CYLINDER;
    *cylinder = (lba / SECTORS_PER_TRACK) / HEADS_PER_CYLINDER;
}