/* KernalOS linker script — GRUB Multiboot2 target
 * Kernel loads at 1MB (0x100000) in 32-bit protected mode.
 */

ENTRY(_start)

SECTIONS {
    . = 0x100000;

    /* Multiboot2 header must appear within the first 32 KB of the image */
    .text : {
        *(.multiboot2)
        *(.text)
    }

    .rodata ALIGN(4096) : {
        *(.rodata)
    }

    .data ALIGN(4096) : {
        *(.data)
    }

    .bss ALIGN(4096) : {
        *(COMMON)
        *(.bss)
    }
}
