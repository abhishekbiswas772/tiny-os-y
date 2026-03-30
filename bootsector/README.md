# Bootsector Folder Guide

This folder contains the first code that runs when the machine boots this project.
Everything here is assembly.

The boot sector is limited to `512` bytes, so each part of the boot process is split into small include files.

## Main Goal Of This Folder

This folder is responsible for:

1. starting in 16-bit real mode
2. printing simple text using BIOS services
3. loading the kernel from disk into memory
4. preparing the CPU for 32-bit protected mode
5. switching into protected mode
6. printing again using direct VGA memory access
7. calling the kernel

## Boot Flow Inside This Folder

```text
BIOS loads boot sector at 31744
   |
   v
bootsector_main.asm
   |
   +--> save boot drive
   |
   +--> make stack
   |
   +--> print real mode message
   |
   +--> load kernel to 4096
   |
   +--> switch_to_pm
            |
            v
        load GDT
            |
            v
        enable protected mode
            |
            v
        BEGIN_PM
            |
            +--> print 32-bit message
            |
            +--> call kernel at 4096
```

### Step 1: BIOS jumps to `bootsector_main.asm`

The BIOS loads the boot sector into memory address `31744` and starts executing it.

The main file:

- saves the boot drive number
- sets up a stack
- prints startup text
- calls the disk loader
- switches to protected mode

### Step 2: `bootsector_print.asm` prints in real mode

This file uses BIOS interrupt `16` to print characters.

Why it works:

- the CPU is still in real mode
- BIOS video services are still available

### Step 3: `bootsector_disc.asm` reads the kernel

This file uses BIOS interrupt `19` to read disk sectors.

The kernel is loaded into memory address `4096`.
In this project, the boot sector asks for `2` sectors.

### Step 4: `bootsector_32_bit_gdt.asm` defines memory rules

Before protected mode is enabled, the CPU needs a valid GDT.

This file defines:

- one null entry
- one code segment
- one data segment

Both useful segments start at memory address `0`.

### Step 5: `bootsector_32_bit_switch.asm` enables protected mode

This file:

1. disables interrupts
2. loads the GDT
3. enables the protected mode bit
4. performs a far jump
5. reloads segment registers
6. moves the stack to address `589824`
7. enters `BEGIN_PM`

### Step 6: `bootsector_32_bit_print.asm` prints in protected mode

This file writes text directly to VGA text memory at address `753664`.

This is different from real mode printing.
Now there is no BIOS helper being used here.

### Step 7: Kernel call

After printing the protected mode message, the boot sector calls the kernel entry point at address `4096`.

That address matches where the kernel was loaded from disk.

## File Relationship Flow Chart

```text
bootsector_main.asm
   |
   +--> bootsector_print.asm
   |
   +--> bootsector_print_hex.asm
   |
   +--> bootsector_disc.asm
   |
   +--> bootsector_32_bit_gdt.asm
   |
   +--> bootsector_32_bit_switch.asm
   |
   +--> bootsector_32_bit_print.asm
```

## File Explanations

### `bootsector_main.asm`

The central boot file.
It includes all helper files and defines both the early real-mode work and the later protected-mode entry point.

### `bootsector_print.asm`

Contains:

- `print`
- `print_nl`

These are small output helpers for 16-bit real mode.

### `bootsector_print_hex.asm`

Contains:

- `print_hex`

This is mainly for debugging values.
It prints them in hexadecimal, but you can treat it as a raw-number display helper.

### `bootsector_disc.asm`

Contains:

- `disk_load`

This reads sectors from the boot disk and checks for errors.

### `bootsector_32_bit_gdt.asm`

Contains:

- the GDT entries
- the GDT descriptor
- code and data segment selector values

### `bootsector_32_bit_switch.asm`

Contains:

- `switch_to_pm`
- `init_pm`

This is the file that changes CPU mode.

### `bootsector_32_bit_print.asm`

Contains:

- `print_string_pm`

This prints by writing straight into display memory.

### `bootsector.bin`

Generated output.
This is the compiled boot sector machine code.

## Important Numbers In Decimal

- boot sector load address: `31744`
- kernel load address: `4096`
- protected mode stack: `589824`
- VGA text memory: `753664`
- boot signature value: `43605`
- one disk sector: `512` bytes

## What To Read First

If this folder feels confusing, use this order:

1. `bootsector_main.asm`
2. `bootsector_print.asm`
3. `bootsector_disc.asm`
4. `bootsector_32_bit_gdt.asm`
5. `bootsector_32_bit_switch.asm`
6. `bootsector_32_bit_print.asm`

That order follows the real execution flow.
