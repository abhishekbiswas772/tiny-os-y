#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "../cpu/types.h"

#define MULTIBOOT2_MAGIC 0x36d76289u

/* Multiboot2 info structure (partial — only fields we use) */
typedef struct {
    u32 type;
    u32 size;
} mb2_tag_t;

typedef struct {
    u32 total_size;
    u32 reserved;
    mb2_tag_t tags[];   /* variable length tag array follows */
} mb2_info_t;

#endif
