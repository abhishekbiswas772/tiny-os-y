#include "ata.h"
#include "../ports/ports.h"

/* ── Port map: Primary ATA channel ──────────────────────────────────────── */
#define ATA_DATA        0x1F0   /* 16-bit data register                     */
#define ATA_ERROR       0x1F1   /* error info (read) / features (write)     */
#define ATA_SECCOUNT    0x1F2   /* sector count                             */
#define ATA_LBA_LO      0x1F3   /* LBA bits  0-7                            */
#define ATA_LBA_MID     0x1F4   /* LBA bits  8-15                           */
#define ATA_LBA_HI      0x1F5   /* LBA bits 16-23                           */
#define ATA_DRIVE_HEAD  0x1F6   /* drive/head select + LBA bits 24-27       */
#define ATA_STATUS      0x1F7   /* status (read)                            */
#define ATA_CMD         0x1F7   /* command (write)                          */
#define ATA_ALT_STATUS  0x3F6   /* alternate status / device control        */

/* Status register bits */
#define ATA_SR_BSY  0x80        /* busy                                     */
#define ATA_SR_DRDY 0x40        /* drive ready                              */
#define ATA_SR_DRQ  0x08        /* data request — data ready for transfer   */
#define ATA_SR_ERR  0x01        /* error                                    */

/* Commands */
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_CACHE_FLUSH   0xE7
#define ATA_CMD_IDENTIFY      0xEC

/* ── Helpers ─────────────────────────────────────────────────────────────── */

#define ATA_TIMEOUT 100000  /* ~200ms on typical hardware */

/* 400ns delay per ATA spec — read ALT_STATUS 4 times (~100ns each). */
static void ata_delay_400ns(void) {
    port_byte_in(ATA_ALT_STATUS);
    port_byte_in(ATA_ALT_STATUS);
    port_byte_in(ATA_ALT_STATUS);
    port_byte_in(ATA_ALT_STATUS);
}

/* Poll until BSY is cleared.  Returns 0 on success, -1 on timeout. */
static int ata_wait_bsy(void) {
    int i;
    for (i = 0; i < ATA_TIMEOUT; i++) {
        if (!(port_byte_in(ATA_ALT_STATUS) & ATA_SR_BSY))
            return 0;
    }
    return -1;
}

/* Poll until DRQ is set (data ready).  Returns 0 on success, -1 on timeout. */
static int ata_wait_drq(void) {
    int i;
    for (i = 0; i < ATA_TIMEOUT; i++) {
        u8 s = port_byte_in(ATA_ALT_STATUS);
        if (s & ATA_SR_ERR) return -1;
        if (s & ATA_SR_DRQ) return 0;
    }
    return -1;
}

/* Send LBA28 command header for primary master. */
static void ata_select_lba28(u32 lba, u8 count) {
    ata_wait_bsy();
    port_byte_out(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); /* LBA mode, master */
    port_byte_out(ATA_SECCOUNT,   count);
    port_byte_out(ATA_LBA_LO,     (u8)(lba));
    port_byte_out(ATA_LBA_MID,    (u8)(lba >> 8));
    port_byte_out(ATA_LBA_HI,     (u8)(lba >> 16));
}

/* ── Public API ──────────────────────────────────────────────────────────── */

int ata_init(void) {
    /* Software reset on the control register */
    port_byte_out(ATA_ALT_STATUS, 0x04);  /* SRST bit */
    ata_delay_400ns();
    port_byte_out(ATA_ALT_STATUS, 0x00);  /* clear reset */
    ata_delay_400ns();

    /* Select primary master */
    port_byte_out(ATA_DRIVE_HEAD, 0xA0);
    if (ata_wait_bsy() != 0)
        return -1;  /* timeout — no drive */

    /* Send IDENTIFY */
    port_byte_out(ATA_CMD, ATA_CMD_IDENTIFY);
    ata_delay_400ns();

    u8 status = port_byte_in(ATA_STATUS);
    if (status == 0x00 || status == 0xFF)
        return -1;   /* no drive present */

    if (ata_wait_bsy() != 0)
        return -1;

    /* If LBA_MID or LBA_HI are non-zero this is an ATAPI device, not ATA */
    if (port_byte_in(ATA_LBA_MID) || port_byte_in(ATA_LBA_HI))
        return -1;

    if (ata_wait_drq() != 0)
        return -1;

    /* Drain the 256-word IDENTIFY buffer — we don't need its content */
    int i;
    for (i = 0; i < 256; i++)
        port_word_in(ATA_DATA);

    return 0;
}

int ata_read_sectors(u32 lba, u8 count, u8 *buf) {
    ata_select_lba28(lba, count);
    port_byte_out(ATA_CMD, ATA_CMD_READ_SECTORS);

    u16 *buf16 = (u16 *)buf;
    int s, i;
    for (s = 0; s < count; s++) {
        ata_wait_bsy();
        ata_wait_drq();

        if (port_byte_in(ATA_STATUS) & ATA_SR_ERR)
            return -1;

        for (i = 0; i < 256; i++)
            buf16[s * 256 + i] = port_word_in(ATA_DATA);
    }
    return 0;
}

int ata_write_sectors(u32 lba, u8 count, u8 *buf) {
    ata_select_lba28(lba, count);
    port_byte_out(ATA_CMD, ATA_CMD_WRITE_SECTORS);

    u16 *buf16 = (u16 *)buf;
    int s, i;
    for (s = 0; s < count; s++) {
        ata_wait_bsy();
        ata_wait_drq();

        for (i = 0; i < 256; i++)
            port_word_out(ATA_DATA, buf16[s * 256 + i]);

        /* Flush write cache after each sector */
        port_byte_out(ATA_CMD, ATA_CMD_CACHE_FLUSH);
        ata_wait_bsy();
    }
    return 0;
}
