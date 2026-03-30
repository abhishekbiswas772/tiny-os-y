# Kernal Folder Guide

This folder contains the code that runs after the boot sector has already:

- loaded the kernel into memory
- switched to 32-bit protected mode
- jumped to the kernel entry point

The folder name is spelled `kernal` in this project, so the documentation keeps that same spelling.

## Main Goal Of This Folder

This folder proves that the bootloader succeeded.

It does that by:

1. starting a tiny 32-bit kernel entry
2. calling a C function
3. writing directly to VGA text memory

If you see the character `X` on the screen, it means the chain worked.

## Kernel Flow Chart

```text
boot sector calls address 4096
   |
   v
kernal_entry.asm starts
   |
   v
call main
   |
   v
kernal_entry.c main()
   |
   v
point to VGA memory 753664
   |
   v
write character X
   |
   v
screen shows X
```

## Execution Order

### `kernal_entry.asm`

This file runs first inside the kernel.

It does:

1. `call main`
2. `jmp $`

Plain English:

- ask the CPU to run the C function `main`
- if `main` ever returns, stop progress and loop forever

Why this file exists:

C code does not magically start on its own in a freestanding kernel.
An assembly entry point is needed first.

### `kernal_entry.c`

This file contains the function `main`.

Right now the code is extremely small:

- it points to VGA text memory at address `753664`
- it writes the character `X`

That means the kernel is not using a library to print.
It is writing directly to display memory.

## File-Wise Notes

### `kernal_entry.asm`

Concepts shown:

- external symbol declaration with `extern main`
- calling a C function from assembly
- halting progress with an infinite loop

### `kernal_entry.c`

Concepts shown:

- freestanding C
- direct memory access with a pointer
- hardware-level output without an operating system

### `simple_program.c`

Your editor tab list mentioned this file, but it is not currently present in the repository.
So it is not part of the current build output.

## Important Concepts Explained

### Why call C from assembly?

The CPU only understands machine instructions.
Assembly is the first simple layer above that.

The bootloader jumps into raw code at a fixed memory address.
The tiny assembly entry file creates a clean handoff into C.

### Why write to memory instead of using `printf`?

Because this project does not have:

- an operating system below it
- a console driver
- a C runtime
- a standard output stream

So the kernel must touch hardware memory directly.

### Why does `main` return type not behave like a normal desktop program?

This is not a normal hosted C program.
There is no operating system waiting to receive an exit code.

This is a freestanding environment.
The code is responsible for everything itself.

## What Success Looks Like

If the whole project works:

1. the boot sector starts
2. the screen messages appear
3. protected mode is entered
4. the kernel runs
5. the top-left VGA cell shows `X`

That final `X` is the smallest possible proof that the kernel is alive.
