# Bootsector — Complete Line-by-Line Survival Guide

This folder is everything that runs BEFORE the kernel.
It is the very first code the machine executes after the BIOS hands control over.

Every file is assembly.
Every concept here is explained from scratch.

---

## What Problem Does This Folder Solve?

When you power on a computer:

1. The CPU has no idea what operating system you have.
2. The BIOS (built-in chip firmware) takes over first.
3. The BIOS looks at your disk and reads the very first 512 bytes.
4. It checks if the last 2 bytes equal 43605. If yes, it treats those 512 bytes as runnable code.
5. It copies those 512 bytes to memory address 31744 and jumps to it.

Your entire boot sector must fit inside those 512 bytes.
But a kernel is much larger than 512 bytes.
So the boot sector's job is to:

1. print a greeting
2. load the kernel from disk into memory (more sectors after the first one)
3. switch the CPU from old 16-bit mode to 32-bit protected mode
4. hand control to the kernel

That is what every file in this folder does.

---

## Boot Flow Chart

```
Power on
   |
   v
BIOS firmware runs — checks hardware
   |
   v
BIOS reads first 512 bytes from disk
copies them to memory address 31744
   |
   v
BIOS checks last 2 bytes == 43605
   |
   v
BIOS jumps to address 31744
   |
   v
bootsector_main.asm starts
CPU is in 16-bit real mode
   |
   +--> save boot drive number
   |
   +--> set up stack at 36864
   |
   +--> print "Starting in 16-bit real mode" (via BIOS interrupt 16)
   |
   +--> load_kernal:
   |      print "Loading kernel into memory"
   |      read 15 sectors from disk into memory address 4096
   |      (uses BIOS interrupt 19 — disk read service)
   |
   +--> call switch_to_pm
            |
            +--> disable CPU interrupts (cli)
            |
            +--> tell CPU where GDT is (lgdt)
            |
            +--> set bit 0 of cr0 to 1 (enter protected mode)
            |
            +--> far jump to flush CPU pipeline
            |
            v
         init_pm (CPU is now 32-bit)
            |
            +--> update all segment registers
            |
            +--> move stack to 589824
            |
            +--> call BEGIN_PM
                    |
                    +--> print "Landed in 32-bit protected mode" (via VGA memory 753664)
                    |
                    +--> call KERNEL at address 4096
                            |
                            v
                         kernel runs
```

---

## File: `bootsector_main.asm` — The Brain of the Boot Sector

This is the main file.
It includes all the other files and ties the entire boot process together.

### Line-by-line walkthrough

```asm
[bits 16]
```
Tell the assembler: generate 16-bit instructions.
The CPU boots in 16-bit real mode, so all instructions must be 16-bit.

```asm
[org 0x7c00]
```
Tell the assembler: assume this code starts at memory address 31744.
The BIOS always loads the boot sector to exactly that address.
If we didn't say this, the assembler would calculate all label addresses wrong
(off by 31744) and jumps and data references would all go to the wrong places.

```asm
KERNAL_OFFSET equ 0x1000
```
Define a constant: 4096.
This is where the kernel will be loaded in memory.
Using a name like `KERNAL_OFFSET` instead of writing 4096 everywhere makes it
easy to change later and easier to read.

```asm
mov [BOOT_DRIVE], dl
```
The BIOS puts the boot drive number into `dl` before jumping to our code.
We save it into a variable called `BOOT_DRIVE`.
Why? Because later when we call the disk read function, we need to tell the BIOS
which disk to read from. Without saving this we'd forget it.

```asm
mov bp, 0x9000
mov sp, bp
```
Set up the stack at memory address 36864.
`bp` gets the address first, then `sp` copies it.
From this point on, `push` and `pop` instructions work.
The stack grows downward in memory from this address.

Why 36864? It is well above the boot sector (31744) and well below the kernel load area.
There is enough room for the stack to grow without crashing into anything.

```asm
mov bx, MSG_REAL_MODE
call print
call print_nl
```
Put the address of the `MSG_REAL_MODE` string into `bx`.
Call the `print` function (defined in `bootsector_print.asm`).
That function reads characters from `bx` one by one and prints them using BIOS.
Then `print_nl` moves the cursor to the next line.

```asm
call load_kernal
```
Jump to the `load_kernal` label below which loads the kernel from disk.

```asm
call switch_to_pm
```
Call the protected mode switch (defined in `bootsector_32_bit_switch.asm`).
After this call, the CPU is in 32-bit mode. We never come back here.

```asm
jmp $
```
This line is never reached because `switch_to_pm` never returns.
But it is a safety net — if somehow execution falls through, stop here forever.

```asm
[bits 16]
load_kernal:
    mov bx, MSG_LOAD_KERNAL
    call print
    call print_nl
    mov bx, KERNAL_OFFSET      ; destination address = 4096
    mov dh, 15                 ; read 15 sectors
    mov dl, [BOOT_DRIVE]       ; from this drive
    call disk_load
    ret
```
This function loads the kernel.
It sets `bx` to 4096 (where kernel will land in memory).
It sets `dh` to 15 (how many sectors to read — enough for any reasonable kernel size).
It reads `[BOOT_DRIVE]` (the drive number saved earlier) into `dl`.
Then calls `disk_load` from `bootsector_disc.asm`.

```asm
[bits 32]
BEGIN_PM:
    mov ebx, MSG_PROT_MODE
    call print_string_pm
    call KERNAL_OFFSET
    jmp $
```
This is the 32-bit entry point.
The CPU jumps here after the mode switch completes.
`[bits 32]` is needed because the CPU is now 32-bit — instructions must match.
Print a message using 32-bit VGA printing.
Then call the kernel at address 4096.
The `jmp $` after it is a safety net — the kernel should never return.

```asm
BOOT_DRIVE      db 0
MSG_REAL_MODE   db "Starting in 16-bit real mode", 0
MSG_PROT_MODE   db "Landed in 32-bit protected mode", 0
MSG_LOAD_KERNAL db "Loading kernel into memory", 0
```
These are the string variables used above.
Each string ends with `, 0` — a zero byte that marks the end of the string.
The print functions loop until they see that zero.

```asm
times 510-($-$$) db 0
dw 0xaa55
```
Pad the file with zero bytes to reach exactly 510 bytes.
Then write the 2-byte boot signature 43605.
Total = 512 bytes, which is exactly one disk sector.

---

## File: `bootsector_print.asm` — Printing in Real Mode

This file gives two functions: `print` and `print_nl`.

### How text printing works in real mode

The BIOS provides a service to print one character at a time.
Interrupt 16 with `ah = 14` ("teletype output") prints whatever character is in `al`.

### `print` — line by line

```asm
print:
    pusha
```
Save all registers.
The function is going to use `ax`, `bx`, etc.
We save them first so the caller's register values are not destroyed.

```asm
print_loop:
    mov al, [bx]
```
`bx` points to the start of the string.
`[bx]` reads 1 byte from that memory address into `al`.
That byte is the next character.

```asm
    cmp al, 0
    je print_done
```
Compare `al` to zero.
If it is zero, we hit the end of the string (the null terminator).
Jump to `print_done`.

```asm
    mov ah, 0x0e
    int 0x10
```
Set `ah` to 14 — that is the "teletype print character" BIOS service.
Run BIOS interrupt 16.
The BIOS sees `ah = 14`, reads the character from `al`, prints it, moves the cursor forward.

```asm
    add bx, 1
    jmp print_loop
```
Move `bx` forward by 1 byte (point to the next character).
Jump back to the top of the loop.

```asm
print_done:
    popa
    ret
```
Restore all registers (undo the `pusha`).
Return to whoever called us.

### `print_nl` — printing a newline

```asm
print_nl:
    pusha
    mov ah, 0x0e
    mov al, 0x0a    ; ASCII 10 = newline (line feed)
    int 0x10
    mov al, 0x0d    ; ASCII 13 = carriage return (move cursor to start of line)
    int 0x10
    popa
    ret
```

A "newline" on old terminals requires two characters:
- Line Feed (10) — moves the cursor down one row
- Carriage Return (13) — moves the cursor back to column 0

Both are needed together for a proper new line.

---

## File: `bootsector_disc.asm` — Reading the Kernel From Disk

This file loads sectors from the disk into memory.
Without this, the kernel binary would never get into RAM and could never run.

### How disk reading works in real mode

BIOS interrupt 19 with `ah = 2` reads sectors from disk.

You tell the BIOS:
- `al` — how many sectors to read
- `ch` — cylinder number (track number, like which ring of the disk)
- `cl` — starting sector number (which sector on that track, starts from 1)
- `dh` — head number (which side of the disk platter)
- `dl` — drive number (0 = floppy, 128 = first hard disk)
- `es:bx` — where in memory to put the data

After the interrupt:
- if the carry flag is set — disk error happened
- `al` = number of sectors actually read

### `disk_load` — line by line

```asm
disk_load:
    pusha
    push dx
```
Save all registers and `dx` separately.
We save `dx` because we need `dh` (sector count) after the BIOS call,
but the BIOS might change `dx`.

```asm
    mov ah, 0x02
```
BIOS disk service 2 = "read sectors."

```asm
    mov al, dh
```
`dh` was set by the caller to the number of sectors we want to read (15 in this project).
Move it into `al` where BIOS expects the sector count.

```asm
    mov cl, 0x02
```
Start reading from sector 2.
Sector 1 is the boot sector itself (already loaded by BIOS).
Sector 2 is where the kernel begins.

```asm
    mov ch, 0x00
    mov dh, 0x00
```
Cylinder 0, head 0.
The kernel is at the very start of the disk.

```asm
    int 0x13
```
Run interrupt 19 — actually perform the disk read.
The BIOS reads the sectors and copies them to wherever `es:bx` points.
(The caller set `bx` to 4096 — the kernel load address.)

```asm
    jc disk_error
```
If the carry flag was set after the interrupt, something went wrong.
Jump to the error handler.

```asm
    pop dx
    cmp al, dh
    jne sector_error
```
Restore our original `dx` (which had the sector count in `dh`).
Compare `al` (sectors actually read) with `dh` (sectors we asked for).
If they don't match, something went wrong.

```asm
    popa
    ret
```
All good. Restore registers and return.

### Error handlers

```asm
disk_error:
    mov bx, DISK_ERROR
    call print
    call print_nl
    mov dh, ah         ; ah contains error code from BIOS
    call print_hex     ; print the error code for debugging
    jmp disk_loop

sector_error:
    mov bx, SECTORS_ERROR
    call print

disk_loop:
    jmp $              ; halt forever — can't continue without the kernel
```

---

## File: `bootsector_32_bit_gdt.asm` — The Memory Layout Table

### What is the GDT?

GDT stands for Global Descriptor Table.
It is a table in memory that tells the CPU how to use memory in protected mode.

In 16-bit real mode, memory access is very primitive — no rules, no protection.
In 32-bit protected mode, the CPU requires a table of "segments."
Each segment describes a region of memory, who can use it, and how.

Think of the GDT as a rulebook for memory in protected mode.
You must hand it to the CPU BEFORE you switch to protected mode.

### This project uses a flat memory model

Instead of dividing memory into complex chunks, this project takes the simplest approach:
- code segment covers ALL of memory from address 0 to 4 gigabytes
- data segment covers ALL of memory from address 0 to 4 gigabytes

Both segments overlay the same space.
This is called a "flat model."
It means you can use any address for any purpose — the CPU won't block you.
It is simple and works fine for a beginner kernel.

### The three required entries

The GDT must start with a null entry (8 zero bytes). The CPU requires this.
Then come the actual segment descriptors.

```asm
gdt_start:
    dd 0x0
    dd 0x0
```
Null entry — 8 bytes of zeros. Required by the CPU spec.
If this is missing the CPU will crash as soon as protected mode starts.

```asm
gdt_code:
    dw 0xffff       ; limit bits 0-15 (65535 = max)
    dw 0x0          ; base bits 0-15 (start at address 0)
    db 0x0          ; base bits 16-23
    db 10011010b    ; access byte
    db 11001111b    ; flags + limit bits 16-19
    db 0x0          ; base bits 24-31
```

The access byte 10011010 in binary, read bit by bit:

| Bit | Value | Meaning |
|-----|-------|---------|
| 7 | 1 | segment is present in memory |
| 6-5 | 00 | ring 0 (kernel privilege — most powerful) |
| 4 | 1 | it's a normal code/data segment (not a system segment) |
| 3 | 1 | executable (this is a code segment) |
| 2 | 0 | not a conforming segment |
| 1 | 1 | readable (code can be read as data) |
| 0 | 0 | not accessed yet |

The flags byte 11001111:
- bits 7-6 `11` — 32-bit mode, granularity is 4096 bytes per unit
  (limit of 65535 units × 4096 bytes = 4 gigabytes coverage)
- bits 5-4 `00` — reserved, must be 0
- bits 3-0 `1111` — upper 4 bits of limit (combined with earlier limit = full range)

```asm
gdt_data:
    ; same span as code but access byte is 10010010b
    db 10010010b    ; present, ring 0, data, writable
```
Data segment: almost identical to code segment but bit 3 = 0 (not executable) and
bit 1 = 1 (writable). This is where variables and stack live.

```asm
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1    ; size of GDT minus 1 (CPU requirement)
    dd gdt_start                   ; address of the GDT
```
This is the structure the `lgdt` instruction reads.
6 bytes: 2 bytes for size, 4 bytes for address.

```asm
CODE_SEG equ gdt_code - gdt_start   ; = 8
DATA_SEG equ gdt_data - gdt_start   ; = 16
```
These are the byte offsets of each descriptor within the GDT.
They are used as "selectors" — the values that go into segment registers.
In protected mode, `cs` doesn't hold an address, it holds one of these selector values.

---

## File: `bootsector_32_bit_switch.asm` — Flipping the CPU to 32-bit Mode

This is the most critical file in the boot sector.
Everything before was setup. This is the actual switch.

### The switch — step by step

```asm
[bits 16]

switch_to_pm:
    cli
```
`cli` = Clear Interrupt flag.
Disable all hardware interrupts.

Why?
The BIOS interrupt handler routines are written for 16-bit real mode.
If a hardware interrupt fires WHILE we are switching modes, the CPU would try to
use a 16-bit handler with 32-bit mode rules. That would crash the system.
So we disable interrupts for the duration of the switch.

```asm
    lgdt [gdt_descriptor]
```
Load the GDT.
`lgdt` reads the 6-byte descriptor at `[gdt_descriptor]` and tells the CPU
where the GDT lives and how big it is.
After this, the CPU knows the memory segment rules for protected mode.

```asm
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
```
This is the single instruction that flips the CPU into protected mode.

`cr0` is the CPU control register.
Bit 0 of `cr0` is called the PE bit (Protection Enable).
Setting it to 1 switches the CPU to protected mode.

We can't write to `cr0` directly with an immediate value.
So we copy `cr0` to `eax`, set bit 0 using OR, then copy it back.

After the last `mov cr0, eax`, the CPU is technically in protected mode.
But we still need to do the far jump.

```asm
    jmp CODE_SEG:init_pm
```
This is a far jump.
A far jump changes both the instruction pointer AND the code segment register (`cs`).

Why is this needed?

When you change `cr0`, the CPU's instruction pipeline might still have old 16-bit
instructions queued up. A far jump forces the CPU to completely flush that pipeline.
It also loads `cs` with `CODE_SEG` (value 8), making it use the new GDT code descriptor.

Without this far jump the CPU would likely crash or execute garbage.

```asm
[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
```
Now we are in 32-bit mode.
`[bits 32]` tells the assembler to generate 32-bit instructions.

All data segment registers (`ds`, `ss`, `es`, `fs`, `gs`) need to be updated.
They are set to `DATA_SEG` (value 16) — the offset of the data descriptor in the GDT.
In protected mode, segment registers hold GDT selector values, not raw addresses.

Before this, they might still hold garbage from real mode.
After this, they all point to the flat data segment that covers all of memory.

```asm
    mov ebp, 0x90000
    mov esp, ebp
```
Set up a new 32-bit stack at memory address 589824.

Why move the stack?
The old real-mode stack at 36864 is fine for small amounts, but we want a bigger,
cleaner area now that we're in 32-bit mode.
589824 is below the BIOS data at the top of low memory but far above the kernel.
There is plenty of room for a deep stack here.

```asm
    call BEGIN_PM
```
Jump into the 32-bit code that prints the protected mode message and calls the kernel.
`BEGIN_PM` is defined back in `bootsector_main.asm` under `[bits 32]`.

---

## File: `bootsector_32_bit_print.asm` — Printing After BIOS Is Gone

Once in protected mode, BIOS interrupts cannot be used.
The BIOS interrupt handlers are 16-bit code.
Calling them from 32-bit mode would crash the CPU.

So to print anything, we write directly to VGA text memory.

### VGA text memory

VGA text memory starts at address 753664.
It is a grid of 80 columns × 25 rows = 2000 character slots.
Each slot is 2 bytes:
- byte 0: ASCII character code
- byte 1: color attribute (15 = white on black)

So to print 'A' at position 0 (top-left):
- write 65 (ASCII 'A') to address 753664
- write 15 (white on black) to address 753665

To print 'B' next to it:
- write 66 to address 753666
- write 15 to address 753667

Each character advances the address by 2.

### `print_string_pm` — line by line

```asm
[bits 32]

VIDEO_MEMORY equ 0xb8000    ; = 753664
WHITE_ON_BLACK equ 0x0f     ; = 15

print_string_pm:
    pusha
    mov edx, VIDEO_MEMORY
```
Save all registers.
Point `edx` at the start of VGA memory (753664).
This is where we'll write the first character.

```asm
print_string_pm_loop:
    mov al, [ebx]
```
`ebx` points to the string (caller puts the address in `ebx`).
Read one character from that address into `al`.

```asm
    mov ah, WHITE_ON_BLACK
```
Put the color attribute (15) into `ah`.
Together, `ax` holds both the character (`al`) and the color (`ah`) in 2 bytes.

```asm
    cmp al, 0
    je print_string_pm_done
```
If the character is 0, we hit the end of the string. Stop.

```asm
    mov [edx], ax
```
Write both bytes (character + color) at once to VGA memory.
This puts one visible character on screen.

```asm
    add ebx, 1
    add edx, 2
    jmp print_string_pm_loop
```
Move `ebx` forward by 1 (next character in string).
Move `edx` forward by 2 (next VGA cell — 2 bytes per cell).
Loop back.

```asm
print_string_pm_done:
    popa
    ret
```
Restore registers and return.

Note: this simple function always starts printing at address 753664 (top-left).
It does not track cursor position or handle newlines.
That is good enough for a single boot message.
The real screen driver in `drivers/screen/screen.c` handles all of that properly.

---

## File: `bootsector_print_hex.asm` — Debugging Helper

This file converts a 16-bit number in `dx` into printable hex digits and displays them.

You don't need to understand this file to understand the boot process.
It is a debugging tool used by the disk error handler.

### How it works (simplified)

It has a buffer `HEX_OUT` that starts as the string `"0x0000"`.
The function fills in the four `0` characters with the actual hex digits.

For each of the 4 hex digits (right to left):
1. Take the lowest 4 bits of `dx`
2. Convert to ASCII: digits 0-9 add 48 (ASCII '0' = 48), letters A-F add 55
3. Store the character into the right position in `HEX_OUT`
4. Rotate `dx` right by 4 bits to expose the next digit

Then print the whole `HEX_OUT` string.

---

## Important Numbers in Decimal

| Number | What it means |
|--------|--------------|
| 512 | one disk sector (bytes) |
| 31744 | boot sector load address (BIOS puts it here) |
| 36864 | early real-mode stack base address |
| 4096 | kernel load address |
| 589824 | 32-bit protected-mode stack base address |
| 753664 | VGA text memory start address |
| 43605 | boot signature (last 2 bytes of boot sector) |
| 16 | BIOS interrupt for screen output |
| 19 | BIOS interrupt for disk reads |
| 15 | VGA color code for white text on black background |

---

## Why We Wrote Things The Way We Did

### Why `[org 31744]` instead of `[org 0]`?

Because the BIOS always loads the boot sector to address 31744.
Every label address the assembler calculates must reflect where the code
actually lives in memory at runtime.
If `org` is 0, the assembler thinks `MSG_REAL_MODE` is at address 0.
But at runtime it's at 31744 + some offset.
All string prints would jump to wrong addresses and print garbage or crash.

### Why save `dl` (boot drive) right away?

The BIOS sets `dl` to the drive number before jumping to our code.
But BIOS interrupt calls overwrite registers including `dl`.
If we called any BIOS function before saving `dl`, we'd lose the drive number forever.
Then the disk load would fail because we wouldn't know which disk to read from.

### Why far jump after setting protected mode bit?

The CPU has an instruction pipeline — it starts fetching the NEXT instruction
before it finishes the current one.
When we set the protected mode bit in `cr0`, the CPU might have already
fetched the next instruction using the OLD real-mode rules.
The far jump forces the CPU to start fresh: throw away anything queued,
reload the code segment with the new protected-mode selector, and continue correctly.

### Why set all segment registers after the jump?

In real mode, `ds`, `ss`, `es` etc. hold raw memory addresses.
In protected mode, they hold "selectors" — offsets into the GDT.
The far jump fixed `cs` (code segment) automatically.
But the other segment registers still have the old real-mode values.
If you try to use data or the stack before fixing them, the CPU will use the wrong GDT entry
and likely crash with a protection fault.

### Why move the stack to 589824?

The old real-mode stack at 36864 overlaps with memory we might use for the kernel or data.
Also, 36864 is quite low and the stack grows downward — it could run into the boot sector.
589824 is a clean area in "conventional memory" above all our code but below the BIOS area.
In a 1MB memory map, 589824 is roughly in the middle — plenty of space above for the stack
to grow without hitting anything.
