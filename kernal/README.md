# Kernel Folder — Complete Beginner Guide

This folder contains the code that runs AFTER the boot sector has done its job.

At the point this code starts running:
- the CPU is in 32-bit protected mode
- the kernel binary has been loaded into memory at address 4096
- the boot sector has jumped to that address

Now it is the kernel's turn to run.

---

## What This Folder Contains

| File | Language | Job |
|------|----------|-----|
| `kernal_entry.asm` | Assembly | First instruction that runs in the kernel |
| `kernal_entry.c` | C | Main kernel function — runs screen demo |

---

## Execution Flow

```
Boot sector calls address 4096
   |
   v
kernal_entry.asm starts
(this is the first bytes at address 4096)
   |
   v
[extern main]    — tells assembler: main() lives in a C file
call main        — call the C function
   |
   v
kernal_entry.c — main() runs
   |
   +--> clear_screen()
   |
   +--> kprint_at("X", 1, 6)            prints X at column 1, row 6
   |
   +--> kprint_at("This text...", 75, 10)  prints near right edge of row 10
   |
   +--> kprint_at("There is a line\nbreak", 0, 20)   prints at col 0, row 20
   |
   +--> kprint("There is a line\nbreak")   continues from cursor
   |
   +--> kprint_at("What happens when we run out of space?", 45, 24)
   |
   v
main() returns
   |
   v
kernal_entry.asm: jmp $
CPU loops forever (halted safely)
```

---

## File: `kernal_entry.asm` — The Bridge Between Boot and C

### Why does this file exist?

The boot sector is assembly.
It does `call KERNAL_OFFSET` which jumps to address 4096 and executes raw machine code there.

C code compiled into a binary can't just start executing from its first byte.
C needs:
- a clean register state
- a valid stack (already set up by the boot sector)
- someone to actually call `main()`

So this tiny assembly file is the first thing at address 4096.
Its only job is to call `main()` and then halt.

### Line-by-line

```asm
[bits 32]
```
The CPU is in 32-bit protected mode when this runs.
Tell the assembler to generate 32-bit instructions.

```asm
[extern main]
```
Tell the assembler: there is a function called `main` that we did NOT define in this file.
It exists in another file (the C file `kernal_entry.c`).
The linker will connect the `call main` below to the actual C function during the build.

```asm
call main
```
Call the C function `main`.
The CPU:
1. pushes the return address (address of the next instruction) onto the stack
2. jumps to wherever `main` is located in memory

The C compiler generated code for `main` and placed it somewhere in the binary.
The linker found it and connected this `call` to that location.

```asm
jmp $
```
If `main` ever returns (which it should not in a kernel), halt here forever.
`$` means "the address of this instruction."
So `jmp $` is an infinite loop at this exact spot.
This prevents the CPU from running into whatever garbage comes after in memory.

---

## File: `kernal_entry.c` — The C Kernel

### Why freestanding C?

This C code is compiled with `-ffreestanding`.
That means:

- there is NO standard library (`stdio.h`, `stdlib.h`, etc. don't exist here)
- there is NO `printf`
- there is NO `malloc`
- there is NO operating system doing things behind the scenes

Everything this kernel can do, it must implement itself.

That is why the `drivers/` folder exists — to provide basic screen and port functions
from scratch.

### The `main()` function

```c
#include "../drivers/screen/screen.h"
```
Include the screen driver header so we can use `clear_screen`, `kprint_at`, and `kprint`.

```c
void main() {
```
Return type is `void` because there is nothing to return to.
No OS is waiting for an exit code.

```c
    clear_screen();
```
Write space characters to every cell in VGA memory.
Reset the cursor to position 0,0 (top-left).
This wipes whatever the boot sector printed.

```c
    kprint_at("X", 1, 6);
```
Print the letter X at column 1, row 6.
Arguments: `kprint_at(string, col, row)`.
This is just a test to prove we can place text at specific coordinates.

```c
    kprint_at("This text spans multiple lines", 75, 10);
```
Print starting at column 75, row 10.
The screen is only 80 columns wide.
Starting at column 75 means only 5 characters fit on that row ("This " barely fits).
The rest wraps to the next row starting at column 0.
This demonstrates what happens when text hits the right edge.

```c
    kprint_at("There is a line\nbreak", 0, 20);
```
Print at column 0, row 20.
The `\n` inside the string is a newline character.
The screen driver handles it by moving to the start of the next row.
So "There is a line" appears on row 20, and "break" appears on row 21.

```c
    kprint("There is a line\nbreak");
```
`kprint` (without `_at`) prints at the current cursor position.
The cursor is wherever the previous print left it — right after "break" on row 21.
So this continues from there: "breakThere is a line" on row 21, then "break" on row 22.

```c
    kprint_at("What happens when we run out of space?", 45, 24);
```
Print at column 45, row 24.
Row 24 is the last row (rows go from 0 to 24).
Starting at column 45 means only 35 characters fit.
The string is 38 characters — the last few don't fit and the screen driver shows an error 'E'
in red at the bottom-right to signal the overflow.

---

## Important Concept: Direct Memory Access for Printing

In a normal desktop program you call `printf("hello")`.
Behind the scenes, the OS, C runtime, and terminal driver do the actual work.

Here there is no OS.
So this kernel talks directly to the VGA chip by writing to memory address 753664.

The screen is 80 columns wide × 25 rows tall = 2000 cells.
Each cell = 2 bytes: one for the character, one for the color.

To calculate where a cell is in memory:

```
offset = (row × 80 + col) × 2
memory address = 753664 + offset
```

Example — to write 'H' in white at column 5, row 3:

```
offset = (3 × 80 + 5) × 2 = (240 + 5) × 2 = 490
address = 753664 + 490 = 754154
memory[754154] = 72      (ASCII 'H')
memory[754155] = 15      (white on black color)
```

This is exactly what `print_char` inside `screen.c` does.

---

## What Success Looks Like

When this kernel runs correctly in QEMU you see:

```
(row 6)   X
(row 10)                                                               This
(row 11) text spans multiple lines
(row 20) There is a line
(row 21) breakThere is a line
(row 22) break
(row 24)                                              What happens when we run out of spE
```

The `E` in red at the very bottom-right is the overflow marker from the screen driver.
That is intentional — it shows what happens when you try to print beyond the screen boundary.
