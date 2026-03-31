#include "kfs.h"
#include "../../drivers/disk/ata.h"
#include "../../drivers/screen/screen.h"
#include "../libc/mem.h"
#include "../libc/string.h"

/* ── Internal helpers ────────────────────────────────────────────────────── */

/* Convert a data-block index (0-based) to the LBA it lives at. */
static u32 block_to_lba(u32 block_idx) {
    return KFS_DATA_LBA + block_idx;
}

/* Read one 512-byte sector into buf. */
static void read_sector(u32 lba, u8 *buf) {
    ata_read_sectors(lba, 1, buf);
}

/* Write one 512-byte sector from buf. */
static void write_sector(u32 lba, u8 *buf) {
    ata_write_sectors(lba, 1, buf);
}

/* ── Superblock ──────────────────────────────────────────────────────────── */

void kfs_read_superblock(kfs_superblock_t *sb) {
    read_sector(1, (u8 *)sb);
}

void kfs_write_superblock(kfs_superblock_t *sb) {
    write_sector(1, (u8 *)sb);
}

/* ── Bitmaps ─────────────────────────────────────────────────────────────── */

/* Allocate the first free bit in the bitmap at `lba`.
 * max_items: how many bits are valid.
 * Returns index or -1 if full. */
static int bitmap_alloc(u32 lba, u32 max_items) {
    u8 bm[KFS_BLOCK_SIZE];
    read_sector(lba, bm);

    u32 i;
    for (i = 0; i < max_items; i++) {
        u32 byte = i / 8;
        u32 bit  = i % 8;
        if (!(bm[byte] & (1u << bit))) {
            bm[byte] |= (1u << bit);
            write_sector(lba, bm);
            return (int)i;
        }
    }
    return -1;
}

static void bitmap_free(u32 lba, u32 n) {
    u8 bm[KFS_BLOCK_SIZE];
    read_sector(lba, bm);
    bm[n / 8] &= ~(1u << (n % 8));
    write_sector(lba, bm);
}

int kfs_alloc_inode(void) { return bitmap_alloc(2, KFS_MAX_INODES); }
void kfs_free_inode(u32 n) { bitmap_free(2, n); }
int  kfs_alloc_block(void) { return bitmap_alloc(3, KFS_MAX_BLOCKS); }
void kfs_free_block(u32 n)  { bitmap_free(3, n); }

/* ── Inodes ──────────────────────────────────────────────────────────────── */

void kfs_read_inode(u32 n, kfs_inode_t *out) {
    u32 lba    = KFS_INODE_TABLE_LBA + (n / 8);   /* 8 inodes per sector */
    u32 offset = (n % 8) * sizeof(kfs_inode_t);   /* byte offset in sector */
    u8  buf[KFS_BLOCK_SIZE];
    read_sector(lba, buf);
    memory_copy(buf + offset, (u8 *)out, sizeof(kfs_inode_t));
}

void kfs_write_inode(u32 n, kfs_inode_t *in) {
    u32 lba    = KFS_INODE_TABLE_LBA + (n / 8);
    u32 offset = (n % 8) * sizeof(kfs_inode_t);
    u8  buf[KFS_BLOCK_SIZE];
    read_sector(lba, buf);
    memory_copy((u8 *)in, buf + offset, sizeof(kfs_inode_t));
    write_sector(lba, buf);
}

/* ── Format / Mount ──────────────────────────────────────────────────────── */

void kfs_format(void) {
    u8 zero[KFS_BLOCK_SIZE];
    memory_set(zero, 0, KFS_BLOCK_SIZE);

    /* Zero superblock placeholder first */
    write_sector(1, zero);

    /* Zero inode bitmap (LBA 2) */
    write_sector(2, zero);

    /* Zero block bitmap (LBA 3) */
    write_sector(3, zero);

    /* Zero inode table (LBA 4 – 35, 32 sectors) */
    u32 i;
    for (i = 0; i < 32; i++)
        write_sector(KFS_INODE_TABLE_LBA + i, zero);

    /* Zero root directory block (data block 0 → LBA 36) */
    write_sector(KFS_DATA_LBA, zero);

    /* Create root inode (inode 0) */
    kfs_inode_t root;
    memory_set((u8 *)&root, 0, sizeof(kfs_inode_t));
    root.type        = KFS_TYPE_DIR;
    root.size        = 0;
    root.blocks[0]   = 0;       /* uses data block 0 */
    root.block_count = 1;
    kfs_write_inode(KFS_ROOT_INODE, &root);

    /* Mark inode 0 and block 0 as used */
    u8 bm[KFS_BLOCK_SIZE];

    read_sector(2, bm);
    bm[0] |= 0x01;              /* bit 0 = inode 0 */
    write_sector(2, bm);

    read_sector(3, bm);
    bm[0] |= 0x01;              /* bit 0 = data block 0 */
    write_sector(3, bm);

    /* Write superblock */
    kfs_superblock_t sb;
    memory_set((u8 *)&sb, 0, sizeof(kfs_superblock_t));
    sb.magic        = KFS_MAGIC;
    sb.version      = 1;
    sb.total_blocks = KFS_MAX_BLOCKS;
    sb.free_blocks  = KFS_MAX_BLOCKS - 1;
    sb.total_inodes = KFS_MAX_INODES;
    sb.free_inodes  = KFS_MAX_INODES - 1;
    sb.block_size   = KFS_BLOCK_SIZE;
    sb.root_inode   = KFS_ROOT_INODE;
    kfs_write_superblock(&sb);
}

int kfs_mount(void) {
    kfs_superblock_t sb;
    kfs_read_superblock(&sb);
    return (sb.magic == KFS_MAGIC) ? 0 : -1;
}

/* ── Directory operations ────────────────────────────────────────────────── */

int kfs_dir_lookup(u32 dir_inode, const char *name) {
    kfs_inode_t inode;
    kfs_read_inode(dir_inode, &inode);

    u8 buf[KFS_BLOCK_SIZE];
    kfs_dirent_t *entries = (kfs_dirent_t *)buf;  /* 16 entries per block */
    u32 b;
    for (b = 0; b < inode.block_count; b++) {
        if (inode.blocks[b] == 0) continue;
        read_sector(block_to_lba(inode.blocks[b]), buf);

        int e;
        for (e = 0; e < 16; e++) {
            if (entries[e].inode == 0) continue;
            if (strcmp(entries[e].name, (char *)name) == 0)
                return (int)entries[e].inode;
        }
    }
    return -1;
}

int kfs_dir_add(u32 dir_inode, const char *name, u32 child_inode, u8 type) {
    kfs_inode_t inode;
    kfs_read_inode(dir_inode, &inode);

    u8 buf[KFS_BLOCK_SIZE];
    kfs_dirent_t *entries = (kfs_dirent_t *)buf;

    /* Search existing blocks for a free slot */
    u32 b;
    for (b = 0; b < inode.block_count; b++) {
        if (inode.blocks[b] == 0) continue;
        read_sector(block_to_lba(inode.blocks[b]), buf);

        int e;
        for (e = 0; e < 16; e++) {
            if (entries[e].inode == 0) {
                /* Free slot found */
                entries[e].inode = child_inode;
                entries[e].type  = type;
                /* Copy name — up to KFS_NAME_MAX chars */
                int k;
                for (k = 0; k < KFS_NAME_MAX && name[k]; k++)
                    entries[e].name[k] = name[k];
                entries[e].name[k] = '\0';
                write_sector(block_to_lba(inode.blocks[b]), buf);
                return 0;
            }
        }
    }

    /* No free slot — allocate new block for this directory */
    if (inode.block_count >= KFS_DIRECT_BLOCKS) return -1;  /* dir too large */

    int new_block = kfs_alloc_block();
    if (new_block == -1) return -1;  /* disk full */

    u8 zero[KFS_BLOCK_SIZE];
    memory_set(zero, 0, KFS_BLOCK_SIZE);
    write_sector(block_to_lba((u32)new_block), zero);

    /* Add entry to new block */
    memory_set(buf, 0, KFS_BLOCK_SIZE);
    entries[0].inode = child_inode;
    entries[0].type  = type;
    int k;
    for (k = 0; k < KFS_NAME_MAX && name[k]; k++)
        entries[0].name[k] = name[k];
    entries[0].name[k] = '\0';
    write_sector(block_to_lba((u32)new_block), buf);

    /* Attach block to inode */
    inode.blocks[inode.block_count] = (u32)new_block;
    inode.block_count++;
    kfs_write_inode(dir_inode, &inode);

    /* Update superblock free count */
    kfs_superblock_t sb;
    kfs_read_superblock(&sb);
    if (sb.free_blocks > 0) sb.free_blocks--;
    kfs_write_superblock(&sb);

    return 0;
}

int kfs_dir_remove(u32 dir_inode, const char *name) {
    kfs_inode_t inode;
    kfs_read_inode(dir_inode, &inode);

    u8 buf[KFS_BLOCK_SIZE];
    kfs_dirent_t *entries = (kfs_dirent_t *)buf;
    u32 b;
    for (b = 0; b < inode.block_count; b++) {
        if (inode.blocks[b] == 0) continue;
        read_sector(block_to_lba(inode.blocks[b]), buf);

        int e;
        for (e = 0; e < 16; e++) {
            if (entries[e].inode != 0 && strcmp(entries[e].name, (char *)name) == 0) {
                entries[e].inode = 0;
                entries[e].name[0] = '\0';
                write_sector(block_to_lba(inode.blocks[b]), buf);
                return 0;
            }
        }
    }
    return -1;
}

void kfs_dir_list(u32 dir_inode) {
    kfs_inode_t inode;
    kfs_read_inode(dir_inode, &inode);

    u8 buf[KFS_BLOCK_SIZE];
    kfs_dirent_t *entries = (kfs_dirent_t *)buf;
    u32 b;
    for (b = 0; b < inode.block_count; b++) {
        if (inode.blocks[b] == 0) continue;
        read_sector(block_to_lba(inode.blocks[b]), buf);

        int e;
        for (e = 0; e < 16; e++) {
            if (entries[e].inode == 0) continue;
            /* Print type prefix then name */
            if (entries[e].type == KFS_TYPE_DIR)
                kprint("[D] ");
            else
                kprint("[F] ");
            kprint(entries[e].name);
            kprint("\n");
        }
    }
}

int kfs_dir_get_entries(u32 dir_inode, char names[][28], u8 types[], int max_items) {
    kfs_inode_t inode;
    kfs_read_inode(dir_inode, &inode);

    u8 buf[KFS_BLOCK_SIZE];
    kfs_dirent_t *entries = (kfs_dirent_t *)buf;
    u32 b;
    int count = 0;
    for (b = 0; b < inode.block_count && count < max_items; b++) {
        if (inode.blocks[b] == 0) continue;
        read_sector(block_to_lba(inode.blocks[b]), buf);

        int e;
        for (e = 0; e < 16 && count < max_items; e++) {
            if (entries[e].inode == 0) continue;
            types[count] = entries[e].type;
            int k;
            for (k = 0; k < KFS_NAME_MAX && entries[e].name[k]; k++) {
                names[count][k] = entries[e].name[k];
            }
            names[count][k] = '\0';
            count++;
        }
    }
    return count;
}

/* ── File operations ─────────────────────────────────────────────────────── */

int kfs_create_file(u32 parent_inode, const char *name) {
    /* Check name doesn't already exist */
    if (kfs_dir_lookup(parent_inode, name) != -1) return -1;

    int ino = kfs_alloc_inode();
    if (ino == -1) return -1;

    kfs_inode_t f;
    memory_set((u8 *)&f, 0, sizeof(kfs_inode_t));
    f.type        = KFS_TYPE_FILE;
    f.size        = 0;
    f.block_count = 0;
    kfs_write_inode((u32)ino, &f);

    if (kfs_dir_add(parent_inode, name, (u32)ino, KFS_TYPE_FILE) != 0) {
        kfs_free_inode((u32)ino);
        return -1;
    }

    kfs_superblock_t sb;
    kfs_read_superblock(&sb);
    if (sb.free_inodes > 0) sb.free_inodes--;
    kfs_write_superblock(&sb);

    return ino;
}

int kfs_create_dir(u32 parent_inode, const char *name) {
    if (kfs_dir_lookup(parent_inode, name) != -1) return -1;

    int block = kfs_alloc_block();
    if (block == -1) return -1;

    int ino = kfs_alloc_inode();
    if (ino == -1) { kfs_free_block((u32)block); return -1; }

    /* Zero the new directory's first data block */
    u8 zero[KFS_BLOCK_SIZE];
    memory_set(zero, 0, KFS_BLOCK_SIZE);
    write_sector(block_to_lba((u32)block), zero);

    kfs_inode_t d;
    memory_set((u8 *)&d, 0, sizeof(kfs_inode_t));
    d.type        = KFS_TYPE_DIR;
    d.size        = 0;
    d.blocks[0]   = (u32)block;
    d.block_count = 1;
    kfs_write_inode((u32)ino, &d);

    if (kfs_dir_add(parent_inode, name, (u32)ino, KFS_TYPE_DIR) != 0) {
        kfs_free_inode((u32)ino);
        kfs_free_block((u32)block);
        return -1;
    }

    kfs_superblock_t sb;
    kfs_read_superblock(&sb);
    if (sb.free_inodes > 0) sb.free_inodes--;
    if (sb.free_blocks  > 0) sb.free_blocks--;
    kfs_write_superblock(&sb);

    return ino;
}

int kfs_read_file(u32 inode_n, u32 offset, u8 *buf, u32 len) {
    kfs_inode_t inode;
    kfs_read_inode(inode_n, &inode);

    if (offset >= inode.size) return 0;
    if (offset + len > inode.size) len = inode.size - offset;

    u32 bytes_read = 0;
    u8  sector_buf[KFS_BLOCK_SIZE];

    while (bytes_read < len) {
        u32 pos       = offset + bytes_read;
        u32 block_idx = pos / KFS_BLOCK_SIZE;
        u32 block_off = pos % KFS_BLOCK_SIZE;

        if (block_idx >= inode.block_count || inode.blocks[block_idx] == 0)
            break;

        read_sector(block_to_lba(inode.blocks[block_idx]), sector_buf);

        u32 to_read = KFS_BLOCK_SIZE - block_off;
        if (to_read > len - bytes_read) to_read = len - bytes_read;

        memory_copy(sector_buf + block_off, buf + bytes_read, (int)to_read);
        bytes_read += to_read;
    }
    return (int)bytes_read;
}

int kfs_write_file(u32 inode_n, u32 offset, const u8 *buf, u32 len) {
    kfs_inode_t inode;
    kfs_read_inode(inode_n, &inode);

    u32 bytes_written = 0;
    u8  sector_buf[KFS_BLOCK_SIZE];

    while (bytes_written < len) {
        u32 pos       = offset + bytes_written;
        u32 block_idx = pos / KFS_BLOCK_SIZE;
        u32 block_off = pos % KFS_BLOCK_SIZE;

        if (block_idx >= KFS_DIRECT_BLOCKS) break;  /* no indirect blocks */

        /* Allocate block if needed */
        if (block_idx >= inode.block_count || inode.blocks[block_idx] == 0) {
            int nb = kfs_alloc_block();
            if (nb == -1) break;  /* disk full */

            u8 zero[KFS_BLOCK_SIZE];
            memory_set(zero, 0, KFS_BLOCK_SIZE);
            write_sector(block_to_lba((u32)nb), zero);

            inode.blocks[block_idx] = (u32)nb;
            if (block_idx >= inode.block_count)
                inode.block_count = block_idx + 1;

            kfs_superblock_t sb;
            kfs_read_superblock(&sb);
            if (sb.free_blocks > 0) sb.free_blocks--;
            kfs_write_superblock(&sb);
        }

        read_sector(block_to_lba(inode.blocks[block_idx]), sector_buf);

        u32 to_write = KFS_BLOCK_SIZE - block_off;
        if (to_write > len - bytes_written) to_write = len - bytes_written;

        memory_copy((u8 *)(buf + bytes_written), sector_buf + block_off, (int)to_write);
        write_sector(block_to_lba(inode.blocks[block_idx]), sector_buf);

        bytes_written += to_write;
    }

    u32 new_end = offset + bytes_written;
    if (new_end > inode.size) inode.size = new_end;
    kfs_write_inode(inode_n, &inode);

    return (int)bytes_written;
}

int kfs_delete(u32 parent_inode, const char *name) {
    int ino = kfs_dir_lookup(parent_inode, name);
    if (ino == -1) return -1;

    kfs_inode_t inode;
    kfs_read_inode((u32)ino, &inode);

    /* Free all data blocks */
    u32 b;
    for (b = 0; b < inode.block_count; b++) {
        if (inode.blocks[b] != 0)
            kfs_free_block(inode.blocks[b]);
    }

    /* Free inode */
    kfs_free_inode((u32)ino);

    /* Remove directory entry */
    kfs_dir_remove(parent_inode, name);

    /* Update superblock */
    kfs_superblock_t sb;
    kfs_read_superblock(&sb);
    sb.free_inodes++;
    sb.free_blocks += inode.block_count;
    kfs_write_superblock(&sb);

    return 0;
}
