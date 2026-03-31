#ifndef ATA_H
#define ATA_H

/* ATA PIO driver — Primary IDE channel (0x1F0)
 *
 * Works for:
 *   - QEMU -hda disk.img  (primary master ATA)
 *   - USB drives that the BIOS exposes as ATA/HDD (USB-HDD boot mode)
 *
 * All sector addresses use 28-bit LBA.
 */

#include "../../cpu/types.h"

/* Detect primary master drive.
 * Returns 0 on success, -1 if no drive found. */
int ata_init(void);

/* Read `count` 512-byte sectors starting at `lba` into `buf`.
 * buf must be at least count * 512 bytes.
 * Returns 0 on success, -1 on error. */
int ata_read_sectors(u32 lba, u8 count, u8 *buf);

/* Write `count` 512-byte sectors starting at `lba` from `buf`.
 * Returns 0 on success, -1 on error. */
int ata_write_sectors(u32 lba, u8 count, u8 *buf);

#endif
