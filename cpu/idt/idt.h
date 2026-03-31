#ifndef IDT_H
#define IDT_H

#include "../types.h"
#define KERNAL_CS 0x08
#define IDT_ENTRIES 256

typedef struct __attribute__((packed))
{
    u16 low_offset;
    u16 sel;
    u8 always0;
    u8 flags;
    u16 high_offset;
} idt_gate_t;

typedef struct __attribute__((packed))
{
    u16 limit;
    u32 base;
} idt_register_t;

extern idt_gate_t idt[IDT_ENTRIES];
extern idt_register_t idt_reg;

void set_idt_gate(int n, u32 handler);
void set_idt();

#endif
