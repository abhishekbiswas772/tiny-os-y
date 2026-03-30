# OS From Scratch — Complete Beginner's Guide

This project builds a tiny operating system from nothing.
It starts at the very first instruction the CPU runs after power-on and ends
with a C kernel printing text to the screen.

No libraries. No OS underneath. Just raw code talking directly to hardware.

---

## What This Project Does (Plain English)

1. The machine powers on.
2. Built-in firmware (BIOS) loads the first 512 bytes from disk into memory.
3. Those 512 bytes are our boot sector — hand-written assembly.
4. The boot sector prints a startup message.
5. The boot sector loads the rest of our code (the kernel) from disk into memory.
6. The boot sector switches the CPU from old 16-bit mode to modern 32-bit mode.
7. The kernel runs and prints text to the screen by writing directly to video memory.

That is the skeleton of every operating system that has ever existed.

---

## Reading Order — Start Here

Read these in order. Each one builds on the last.

```
1. basic asm to survive/README.md    ← start here if you know nothing about assembly
2. bootsector/README.md              ← how the machine boots
3. drivers/README.md                 ← how screen and port I/O work
4. kernal/README.md                  ← how the kernel entry works
```

Then read the actual source files:

```
5. bootsector/bootsector_main.asm
6. bootsector/bootsector_print.asm
7. bootsector/bootsector_disc.asm
8. bootsector/bootsector_32_bit_gdt.asm
9. bootsector/bootsector_32_bit_switch.asm
10. bootsector/bootsector_32_bit_print.asm
11. drivers/ports/ports.h
12. drivers/ports/ports.c
13. drivers/screen/screen.h
14. drivers/screen/screen.c
15. kernal/kernal_entry.asm
16. kernal/kernal_entry.c
```

---

## Project Folder Map

```
Kernal/
├── Readme.md                    ← you are here
├── Makefile                     ← build script
│
├── basic asm to survive/
│   └── README.md                ← assembly survival guide + inline C ASM explained
│
├── bootsector/
│   ├── README.md                ← complete boot process guide
│   ├── bootsector_main.asm      ← main boot file — ties everything together
│   ├── bootsector_print.asm     ← 16-bit BIOS text printing
│   ├── bootsector_print_hex.asm ← debugging helper (print numbers)
│   ├── bootsector_disc.asm      ← disk reading (loads kernel from disk)
│   ├── bootsector_32_bit_gdt.asm    ← Global Descriptor Table for protected mode
│   ├── bootsector_32_bit_switch.asm ← switch CPU from 16-bit to 32-bit
│   └── bootsector_32_bit_print.asm  ← 32-bit VGA printing (no BIOS)
│
├── drivers/
│   ├── README.md                ← drivers explained from scratch
│   ├── ports/
│   │   ├── ports.h              ← port I/O declarations
│   │   └── ports.c              ← inline assembly to read/write hardware ports
│   └── screen/
│       ├── screen.h             ← screen function declarations
│       └── screen.c             ← VGA text driver (cursor, print, clear)
│
└── kernal/
    ├── README.md                ← kernel entry explained
    ├── kernal_entry.asm         ← first instruction in kernel, calls C main()
    └── kernal_entry.c           ← C kernel entry point
```

---

## The Full Boot Flow

```
Power on
   |
   v
BIOS (built-in chip firmware) starts
   does: hardware check, finds bootable disk
   |
   v
BIOS reads first 512 bytes from disk
   copies to memory address 31744
   checks last 2 bytes == 43605 (boot signature)
   |
   v
BIOS jumps to address 31744
   |
   v
bootsector_main.asm — CPU in 16-bit real mode
   |
   +--> mov [BOOT_DRIVE], dl          save which disk we booted from
   +--> mov bp/sp, 36864              set up the stack
   +--> call print                    "Starting in 16-bit real mode"
   +--> call load_kernal              read 15 sectors into address 4096
   +--> call switch_to_pm
            |
            +--> cli                  disable interrupts
            +--> lgdt                 tell CPU where GDT is
            +--> set cr0 bit 0 = 1    enable protected mode
            +--> far jump             flush CPU pipeline, load new code segment
            |
            v
         init_pm — CPU in 32-bit protected mode
            |
            +--> update segment registers
            +--> move stack to 589824
            +--> call BEGIN_PM
                    |
                    +--> print "Landed in 32-bit protected mode" (writes to VGA memory)
                    +--> call address 4096 (the kernel)
                            |
                            v
                         kernal_entry.asm
                            call main()
                            |
                            v
                         kernal_entry.c — main()
                            clear_screen()
                            kprint_at / kprint calls
                            text appears on screen
```

---

## Build and Run

### Build everything

```sh
make
```

### Run in QEMU (virtual machine)

```sh
make run
```

### Debug with GDB attached to QEMU

```sh
make debug
```

Or press **F5** in VSCode (uses `.vscode/launch.json`).

### Clean all generated files and rebuild fresh

```sh
make clean && make
```

---

## Important Memory Addresses (All Decimal)

| Address | What lives there |
|---------|-----------------|
| 31744 | boot sector — BIOS loads it here |
| 36864 | early stack — used during 16-bit boot |
| 4096 | kernel — loaded here from disk, runs from here |
| 589824 | protected-mode stack — after the 32-bit switch |
| 753664 | VGA text memory — write here to print to screen |

---

## Important Port Numbers (All Decimal)

| Port | What it controls |
|------|-----------------|
| 948 | VGA cursor control register selector |
| 949 | VGA cursor data register |

---

## Key Concepts You Will Learn From This Project

| Concept | Where it's used |
|---------|----------------|
| Assembly language | bootsector/, kernal/kernal_entry.asm |
| BIOS interrupts | bootsector_print.asm, bootsector_disc.asm |
| 16-bit real mode | all bootsector code before switch |
| 32-bit protected mode | bootsector_32_bit_switch.asm and after |
| Global Descriptor Table (GDT) | bootsector_32_bit_gdt.asm |
| VGA text memory | bootsector_32_bit_print.asm, drivers/screen/ |
| Hardware port I/O | drivers/ports/ |
| Inline C assembly | drivers/ports/ports.c |
| Freestanding C | drivers/screen/screen.c, kernal/kernal_entry.c |
| Linker scripts and build systems | Makefile |

---

## Hex-to-Decimal Reference (Source Uses Hex, Guides Use Decimal)

| Hex | Decimal | Used for |
|-----|---------|---------|
| 0x7C00 | 31744 | boot sector load address |
| 0x1000 | 4096 | kernel load address |
| 0x9000 | 36864 | early stack |
| 0x90000 | 589824 | protected-mode stack |
| 0xB8000 | 753664 | VGA text memory |
| 0xAA55 | 43605 | boot signature |
| 0x10 | 16 | BIOS screen interrupt |
| 0x13 | 19 | BIOS disk interrupt |
| 0x0f | 15 | VGA color: white on black |
| 0xf4 | 244 | VGA color: red on white |
| 0x3d4 | 948 | VGA cursor control port |
| 0x3d5 | 949 | VGA cursor data port |
