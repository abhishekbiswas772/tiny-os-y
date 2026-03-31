#ifndef KFS_H
#define KFS_H

/* KernalFS — simple block-based file system
 *
 * Disk layout (512-byte sectors = blocks):
 *
 *   LBA 0        : reserved (MBR / left empty for safety)
 *   LBA 1        : Superblock
 *   LBA 2        : Inode bitmap  (4096 bits  →  up to 256 inodes used)
 *   LBA 3        : Block bitmap  (4096 bits  →  up to 4096 data blocks)
 *   LBA 4  – 35  : Inode table  (32 sectors × 8 inodes × 64 bytes)
 *   LBA 36 – 4131: Data blocks  (4096 × 512 bytes = 2 MB)
 */

#include "../../cpu/types.h"

/* ── Constants ────────────────────────────────────────────────────────────── */
#define KFS_MAGIC            0x4B465321u   /* "KFS!" */
#define KFS_BLOCK_SIZE       512
#define KFS_MAX_INODES       256
#define KFS_MAX_BLOCKS       4096
#define KFS_INODE_TABLE_LBA  4
#define KFS_DATA_LBA         36
#define KFS_ROOT_INODE       0
#define KFS_DIRECT_BLOCKS    10
#define KFS_NAME_MAX         27            /* max filename chars (no null) */

/* ── Inode types ──────────────────────────────────────────────────────────── */
#define KFS_TYPE_FREE  0
#define KFS_TYPE_FILE  1
#define KFS_TYPE_DIR   2

/* ── On-disk structures ───────────────────────────────────────────────────── */

/* Superblock (512 bytes, lives at LBA 1) */
typedef struct {
    u32 magic;
    u32 version;
    u32 total_blocks;
    u32 free_blocks;
    u32 total_inodes;
    u32 free_inodes;
    u32 block_size;
    u32 root_inode;
    u8  pad[480];            /* pad to exactly 512 bytes */
} __attribute__((packed)) kfs_superblock_t;

/* Inode (64 bytes; 8 fit in one 512-byte sector) */
typedef struct {
    u8  type;                           /*  1 byte  */
    u8  pad0[3];                        /*  3 bytes */
    u32 size;                           /*  4 bytes — file size in bytes    */
    u32 blocks[KFS_DIRECT_BLOCKS];      /* 40 bytes — data block indices    */
    u32 block_count;                    /*  4 bytes                         */
    u8  pad1[12];                       /* 12 bytes — total = 64            */
} __attribute__((packed)) kfs_inode_t;

/* Directory entry (32 bytes; 16 fit in one 512-byte block) */
typedef struct {
    u32  inode;                         /*  4 bytes — 0 means free slot     */
    char name[KFS_NAME_MAX];            /* 27 bytes                         */
    u8   type;                          /*  1 byte  — total = 32            */
} __attribute__((packed)) kfs_dirent_t;

/* ── API ──────────────────────────────────────────────────────────────────── */

/* Format: write empty KFS to disk (destructive). */
void kfs_format(void);

/* Mount: read superblock, validate magic.
 * Returns 0 on success, -1 if disk is not formatted. */
int kfs_mount(void);

/* Superblock */
void kfs_read_superblock(kfs_superblock_t *sb);
void kfs_write_superblock(kfs_superblock_t *sb);

/* Bitmaps — return allocated index or -1 if full */
int  kfs_alloc_inode(void);
void kfs_free_inode(u32 n);
int  kfs_alloc_block(void);
void kfs_free_block(u32 n);

/* Inodes */
void kfs_read_inode(u32 n, kfs_inode_t *out);
void kfs_write_inode(u32 n, kfs_inode_t *in);

/* Directory operations */
int  kfs_dir_lookup(u32 dir_inode, const char *name);
int  kfs_dir_add(u32 dir_inode, const char *name, u32 child_inode, u8 type);
int  kfs_dir_remove(u32 dir_inode, const char *name);
void kfs_dir_list(u32 dir_inode);
int  kfs_dir_get_entries(u32 dir_inode, char names[][28], u8 types[], int max_items);

/* File operations */
int  kfs_create_file(u32 parent_inode, const char *name);
int  kfs_create_dir(u32 parent_inode, const char *name);
int  kfs_read_file(u32 inode, u32 offset, u8 *buf, u32 len);
int  kfs_write_file(u32 inode, u32 offset, const u8 *buf, u32 len);
int  kfs_delete(u32 parent_inode, const char *name);

#endif
