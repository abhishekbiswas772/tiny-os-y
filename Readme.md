# Kernal Project Guide

This project is a very small operating system learning project.
It starts with a boot sector written in assembly, switches the CPU from 16-bit mode to 32-bit mode, loads a tiny kernel into memory, and then runs C code.

This guide is written for a beginner.
Where the source code uses hexadecimal numbers, this document explains them mainly in decimal.
Example:

- `0x7C00` means `31744`
- `0x1000` means `4096`
- `0xB8000` means `753664`
- `0xAA55` means `43605`

## What This Project Does

When you run this project, this is the overall flow:

1. The BIOS reads the first 512 bytes from the disk image.
2. Those 512 bytes are the boot sector.
3. The boot sector starts in 16-bit real mode.
4. The boot sector prints messages using the BIOS.
5. The boot sector loads the kernel from disk into memory address `4096`.
6. The boot sector builds a GDT, enables protected mode, and jumps into 32-bit code.
7. The 32-bit code prints a message by writing directly to screen memory.
8. The boot sector calls the kernel entry point at memory address `4096`.
9. The kernel C code writes `X` to the top-left corner of the VGA text screen.

## Overall Flow Chart

```text
Power On
   |
   v
BIOS starts
   |
   v
BIOS reads first 512 bytes
from disk into memory 31744
   |
   v
bootsector_main.asm starts
in 16-bit real mode
   |
   v
print startup message
using BIOS interrupt 16
   |
   v
load kernel from disk
into memory 4096
   |
   v
prepare GDT
   |
   v
enable 32-bit protected mode
   |
   v
print protected mode message
using VGA memory 753664
   |
   v
call kernel entry at 4096
   |
   v
kernal_entry.asm
   |
   v
main() in kernal_entry.c
   |
   v
write X to screen
```

## Build Flow Chart

```text
Makefile
   |
   +--> assemble bootsector_main.asm
   |        |
   |        v
   |    bootsector.bin
   |
   +--> assemble kernal_entry.asm
   |        |
   |        v
   |    kernal_entry.o
   |
   +--> compile kernal_entry.c
   |        |
   |        v
   |    kernal_entry.o from C files
   |
   +--> link kernel objects
   |        |
   |        v
   |    kernal.bin
   |
   +--> join bootsector.bin + kernal.bin
            |
            v
        os-image.bin
```

## Folder-Wise Summary

### `bootsector/`

This folder contains all the assembly code that runs first.
Its job is to:

- start in 16-bit real mode
- print startup text
- load extra sectors from disk
- switch the CPU to 32-bit protected mode
- jump into the kernel

Important idea:
The BIOS only loads one sector at first.
One sector is `512` bytes.
That is too small for a real kernel, so the boot sector must load more data manually.

### `kernal/`

This folder contains the kernel entry code.
Its job is to:

- provide a tiny 32-bit assembly entry point
- call the C function `main`
- write something to the screen from C

Important idea:
The kernel is not using any operating system services.
There is no `printf`, no files, no normal standard library setup.
It directly touches hardware memory.

### `basic asm to survive/`

This folder is new.
It is a beginner survival guide for assembly and boot code.
It explains:

- registers
- memory
- stack
- BIOS interrupts
- protected mode
- VGA text memory
- why addresses matter

This folder is meant to be read slowly while looking at the real source files.

## File-Wise Summary

### Root files

#### `Makefile`

This is the build script for the whole project.

It knows how to:

- assemble the boot sector
- assemble the kernel entry assembly file
- compile the kernel C file
- link the kernel
- join boot sector and kernel into one disk image
- run the image in QEMU
- start a debug session
- build a cross compiler

Important process inside the `Makefile`:

1. `bootsector/bootsector_main.asm` is assembled into a flat binary.
2. `kernal/kernal_entry.asm` is assembled into an ELF object file.
3. `kernal/*.c` files are compiled into object files.
4. The kernel object files are linked so the kernel starts at address `4096`.
5. The boot sector binary and kernel binary are concatenated into `os-image.bin`.

#### `Readme.md`

This file is the main explanation for the whole repository.

### Files inside `bootsector/`

#### `bootsector/bootsector_main.asm`

This is the main boot sector file.
It is the first project code that the CPU runs after the BIOS jumps to it.

It does these jobs:

1. tells the assembler the code will run at memory address `31744`
2. saves the boot drive number
3. creates a stack
4. prints a message in 16-bit mode
5. loads the kernel from disk into memory address `4096`
6. switches to 32-bit protected mode
7. prints a message in 32-bit mode
8. calls the kernel entry point
9. fills the rest of the sector with zeros
10. places the boot signature `43605` at the end

Important lines:

- `[org 0x7c00]` means "assume this code starts at `31744`"
- `KERNAL_OFFSET equ 0x1000` means kernel will be loaded at `4096`
- `times 510-($-$$) db 0` pads the boot sector to the correct size
- `dw 0xaa55` writes the required boot signature

#### `bootsector/bootsector_print.asm`

This file contains 16-bit text printing helpers.

It uses BIOS video interrupt `16` in decimal form? Not exactly.
The source uses `int 0x10`, which is interrupt `16` in decimal.

What it does:

- `print` reads one character at a time from memory and prints it
- `print_nl` prints a new line

Important concept:
In real mode, the BIOS is still available, so the bootloader can ask the BIOS to print characters.

#### `bootsector/bootsector_print_hex.asm`

This file prints a 16-bit value stored in register `dx`.

The current source prints in hexadecimal because low-level programming often uses that format.
If you do not understand hex, think of this file as:

- take a raw number
- split it into small parts
- convert each part to a printable character
- show the result on screen

You do not need to master this file first to understand the boot process.
It is mainly a debugging helper.

#### `bootsector/bootsector_disc.asm`

This file loads sectors from disk.

What it does:

1. prepares BIOS disk read registers
2. asks the BIOS to read sectors
3. checks whether the read succeeded
4. prints an error if something went wrong

Important concept:
The BIOS initially loads only one sector.
This helper loads the next sectors containing the kernel.

The source uses `int 0x13`, which is interrupt `19` in decimal.
That BIOS interrupt handles disk operations.

#### `bootsector/bootsector_32_bit_gdt.asm`

This file defines the Global Descriptor Table, also called the GDT.

Plain-English meaning:
The GDT is a table that tells the CPU how memory should be viewed in protected mode.

This project uses a flat memory model:

- code segment starts at `0`
- data segment starts at `0`
- both are allowed to cover a very large memory range

That makes later code much simpler.

#### `bootsector/bootsector_32_bit_switch.asm`

This file contains the mode-switch logic.

It does these jobs:

1. disables interrupts
2. loads the GDT
3. turns on the protected mode bit in control register `cr0`
4. performs a far jump
5. reloads segment registers
6. moves the stack to a larger memory area
7. calls the 32-bit label `BEGIN_PM`

Important concept:
A far jump is needed because after changing CPU mode, the instruction pipeline and code segment must be refreshed correctly.

#### `bootsector/bootsector_32_bit_print.asm`

This file prints text in 32-bit protected mode.

At this stage the BIOS is no longer used.
Instead, the code writes directly to VGA text memory at address `753664`.

Each screen cell uses `2` bytes:

- first byte = character
- second byte = color

#### `bootsector/bootsector.bin`

This is the generated boot sector binary.
It is not hand-written source code.
It is the machine code output produced by NASM.

#### `bootsector/README.md`

This file explains the boot sector folder in plain English.

### Files inside `kernal/`

#### `kernal/kernal_entry.asm`

This is the first kernel instruction that runs after the boot sector jumps into the kernel.

It does only two things:

1. call the C function `main`
2. loop forever if `main` returns

Important concept:
The boot sector can jump into raw machine code, but C needs a small assembly entry point first.

#### `kernal/kernal_entry.c`

This is the C side of the kernel.

Right now it is extremely small.
It creates a pointer to VGA memory address `753664` and stores the character `X` there.

So if everything works, the screen shows `X` in the first character position.

#### `kernal/simple_program.c`

This file is not currently present in the repository, even though your editor tab list mentions it.
So it is not part of the current build.

## Overall Process In Detail

### Phase 1: BIOS starts everything

When the computer powers on, your boot sector is not running yet.
The BIOS runs first.

The BIOS:

- checks hardware
- finds a bootable device
- reads the first `512` bytes
- copies those bytes into memory address `31744`
- jumps to that address

That means your bootloader must fit inside one sector and end with the boot signature.

### Phase 2: 16-bit real mode bootloader starts

Your boot sector code begins in old-style 16-bit real mode.

That means:

- registers are mainly used in their 16-bit form
- BIOS interrupts are available
- memory access is limited and old-fashioned

The bootloader first saves the boot drive number from register `dl`.
That is important because later disk reads need to know which disk to read from.

Then it creates a stack.
The stack is temporary working memory used for:

- saving registers
- return addresses
- temporary values

In this project the early stack starts at address `36864`.

### Phase 3: Printing in real mode

The boot sector prints startup messages using BIOS interrupt `16`.

This is possible only because the CPU is still in real mode.
The BIOS provides helper routines for screen output and disk reads.

### Phase 4: Loading the kernel

The boot sector then loads `2` sectors from disk into memory address `4096`.

Why `4096`?

- it is far away from the boot sector at `31744`
- it gives the kernel its own safe place in memory
- it is simple to remember

At this point:

- boot sector code is at `31744`
- kernel binary is loaded at `4096`

The bootloader is still running from its own location.
It is only copying the kernel somewhere else for later execution.

### Phase 5: Preparing protected mode

The boot sector defines a GDT.

Why is this needed?

Because protected mode expects the CPU to know how code and data segments are defined.

Without a valid GDT, protected mode code would not run correctly.

This project keeps the layout simple:

- one null entry
- one code segment entry
- one data segment entry

### Phase 6: Switching to 32-bit protected mode

This is the most important transition in the project.

The bootloader:

1. disables interrupts
2. loads the GDT
3. sets bit `0` of `cr0`
4. performs a far jump
5. reloads segment registers
6. creates a new 32-bit stack at address `589824`

Why move the stack?

Because after switching mode, you want a larger, cleaner stack area that is not crowded near the boot sector.

### Phase 7: Printing in 32-bit mode

In protected mode, BIOS helpers are no longer being used here.
So the code writes directly to VGA memory at `753664`.

This is a very important systems programming idea:

- before an operating system exists, your code talks directly to hardware memory

### Phase 8: Jumping into the kernel

The boot sector calls the kernel entry point at address `4096`.

That works because:

- the kernel was loaded there earlier
- the linker also placed the kernel to run from that same address

So the boot sector and linker agree about where the kernel lives.

### Phase 9: Kernel C code runs

The assembly file `kernal_entry.asm` calls `main`.
Then `kernal_entry.c` writes `X` to VGA memory.

That proves:

- the bootloader loaded the kernel correctly
- the mode switch worked
- the kernel entry point worked
- C code can now execute

## Important Components Explained

### BIOS

The BIOS is firmware provided by the machine.
It exists before your OS.
It helps with very early boot tasks like:

- reading the disk
- writing text in real mode
- giving the boot drive number

### Boot Sector

The boot sector is the first disk sector.
It is exactly `512` bytes.
The BIOS loads it automatically if it ends with the correct signature.

### Stack

The stack is a memory area used in a last-in, first-out style.

Typical stack uses:

- function calls
- return addresses
- saving registers with `push` and `pop`

### Registers

Registers are tiny storage locations inside the CPU.

Examples in this project:

- `ax`, `bx`, `cx`, `dx` for 16-bit work
- `eax`, `ebx`, `edx`, `esp` for 32-bit work
- `cr0` as a control register

### GDT

The GDT tells the CPU which memory segments exist in protected mode.
In this project it is used to create a simple flat layout.

### Protected Mode

Protected mode is a more modern CPU mode than real mode.
It allows:

- 32-bit registers
- better memory handling
- more structured system programming

### VGA Text Memory

This is a memory region used by old text mode displays.
If you write characters there, they appear on the screen.

Address used here:

- `753664`

## Build And Run

### Build the OS image

```sh
make
```

### Run it in QEMU

```sh
make run
```

### Start debug mode

```sh
make debug
```

### Clean generated files

```sh
make clean
```

## Final Beginner Notes

If you are new to assembly, do not try to understand every line at once.
Read the project in this order:

1. `Readme.md`
2. `basic asm to survive/README.md`
3. `bootsector/README.md`
4. `bootsector/bootsector_main.asm`
5. `kernal/README.md`
6. `kernal/kernal_entry.c`

That order will make the code much easier to understand.
