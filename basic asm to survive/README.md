# Basic ASM To Survive

This folder is a beginner guide.
It is not the boot code itself.
It exists to help you read the real source without feeling lost.

This guide uses plain English and mostly decimal numbers.
Hexadecimal is mentioned only when necessary because the source files use it.

## First Truth: Assembly Is Not Magic

Assembly is just a very direct way of telling the CPU what to do.

Examples of the kind of things assembly says:

- move a value into a register
- copy a byte from memory
- jump to another instruction
- call a function
- return from a function

That is all.
The language feels hard mostly because it is very close to the hardware.

## The Four Things You Must Understand First

If you understand these four things, the rest becomes much easier:

1. registers
2. memory
3. the stack
4. jumps and calls

## Basic Execution Flow Chart

```text
CPU starts an instruction
   |
   v
read instruction from memory
   |
   v
instruction changes register,
memory, stack, or jump target
   |
   v
next instruction runs
   |
   v
program continues
```

## Registers

Registers are tiny storage boxes inside the CPU.
They are much faster than normal memory.

In this project you will see:

- `ax`, `bx`, `cx`, `dx`
- `sp`, `bp`
- `eax`, `ebx`, `edx`, `esp`, `ebp`
- `ds`, `ss`, `es`, `fs`, `gs`
- `cr0`

### Simple meaning of common registers

#### `ax`

A general-purpose register.
Often used for calculations or BIOS call setup.

#### `bx`

Often used as a pointer to data in this project.
For example, `bx` may contain the address of a string.

#### `cx`

Often used as a counter.

#### `dx`

Often used for extra data or hardware-related values.
In this project it is used when printing a number or working with disk logic.

#### `sp`

Stack pointer.
This tells the CPU where the current top of the stack is.

#### `bp`

Base pointer.
In this project it is used to help initialize the stack.

#### `eax`, `ebx`, `edx`

These are the 32-bit versions of the smaller registers.
When the CPU switches to 32-bit mode, these become the normal working size.

#### `cr0`

Control register 0.
This register contains important CPU control bits.
One of those bits turns protected mode on.

## Memory

Memory is just a huge list of numbered locations.
Each location has an address.

Example idea:

- address `1000` holds one byte
- address `1001` holds the next byte
- address `1002` holds the next byte

The CPU can:

- read from an address
- write to an address
- jump to an address and start executing code there

In this project, some very important memory addresses are:

- `31744` for the boot sector start
- `4096` for the kernel load address
- `589824` for the protected mode stack
- `753664` for VGA text memory

## What `[something]` Means

In NASM assembly, brackets usually mean:

"use the memory at this address"

Example:

```asm
mov al, [bx]
```

Plain English:

- look at the memory address stored in `bx`
- read one byte from there
- place that byte into `al`

Without brackets:

```asm
mov bx, 4096
```

This means:

- put the number `4096` itself into `bx`

With brackets:

```asm
mov al, [4096]
```

This means:

- go to memory address `4096`
- read the byte stored there

## The Stack

The stack is a special memory area used like a vertical pile.

Last value pushed in:

- comes out first

That is why people say stack behavior is last-in, first-out.

### Stack Flow Chart

```text
stack starts empty
   |
   v
push value A
   |
   v
push value B
   |
   v
top of stack is B
   |
   v
pop
   |
   v
B comes out first
   |
   v
pop again
   |
   v
A comes out next
```

### Why the stack matters

The CPU uses the stack for:

- return addresses during function calls
- temporarily saving register values
- local working data

### Instructions you should know

#### `push something`

Put a value onto the stack.

#### `pop something`

Take the top value off the stack and place it somewhere.

#### `call label`

This does two things:

1. stores the return address on the stack
2. jumps to the target code

#### `ret`

This returns from a function by popping the saved return address from the stack.

That is why the stack must be valid before calling functions.

## Jumps

Jumps change which instruction runs next.

### `jmp label`

Unconditional jump.
The CPU always jumps there.

### `je label`

Jump if equal.
This usually happens after a compare instruction says two values matched.

### `jne label`

Jump if not equal.

### `jmp $`

This means:

- jump to the current location forever

It creates an infinite loop.
It is often used when there is nowhere else to go yet.

## Compare Instructions

Example:

```asm
cmp al, 0
je done
```

Plain English:

- compare `al` with `0`
- if they are equal, jump to `done`

This is very common when walking through a string that ends with a zero byte.

## Real Mode Vs Protected Mode

This is one of the most important ideas in the project.

```text
BIOS boot
   |
   v
16-bit real mode
   |
   +--> BIOS printing works
   +--> BIOS disk reads work
   |
   v
load kernel and prepare GDT
   |
   v
switch CPU mode
   |
   v
32-bit protected mode
   |
   +--> use 32-bit registers
   +--> write directly to VGA memory
```

### Real mode

This is the CPU mode used right after BIOS boot.

Simple features:

- old 16-bit style
- BIOS interrupts available
- limited and awkward memory model

In this mode, your boot sector can ask the BIOS for help.

### Protected mode

This is a more advanced CPU mode.

Simple features:

- 32-bit registers
- better memory handling
- foundation for real operating system work

In this project, once protected mode starts, printing is done by writing directly to VGA memory.

## BIOS Interrupts

An interrupt here is like calling a built-in firmware service.

### Screen output

The source uses `int 0x10`.
That is interrupt `16` in decimal.

This is used for video services in real mode.

### Disk read

The source uses `int 0x13`.
That is interrupt `19` in decimal.

This is used for disk services in real mode.

Important:
These BIOS helpers are useful early in boot, but they are not how a full OS should work forever.

## VGA Text Memory

The project writes to VGA text memory at address `753664`.

That memory is laid out in text cells.
Each cell uses `2` bytes:

1. character byte
2. color byte

So if the code writes:

- character `X`
- color value `15`

then the screen shows `X` in white on black.

## Why Hexadecimal Appears In The Source

You asked for decimal explanations, which is the right choice for learning the process first.

Still, the source uses hex because:

- memory addresses are traditionally written that way
- groups of `4` bits match one hex digit
- low-level tools and docs use hex heavily

You do not need to mentally convert everything immediately.
A safe beginner method is:

1. read the decimal meaning first
2. keep the hex form in parentheses
3. get used to the pattern slowly

Useful examples from this project:

- `0x7C00` = `31744`
- `0x1000` = `4096`
- `0x9000` = `36864`
- `0x90000` = `589824`
- `0xB8000` = `753664`
- `0xAA55` = `43605`

## Reading Real Project Lines In Plain English

### Example 1

```asm
mov [BOOT_DRIVE], dl
```

Plain English:

- take the value currently in `dl`
- store it in the memory variable named `BOOT_DRIVE`

Meaning in the project:

- save which disk the BIOS booted from

### Example 2

```asm
mov bp, 0x9000
mov sp, bp
```

Plain English:

- put `36864` into `bp`
- copy that value into `sp`

Meaning in the project:

- start the early stack at memory address `36864`

### Example 3

```asm
mov bx, MSG_REAL_MODE
call print
```

Plain English:

- put the address of the message string into `bx`
- call the print function

Meaning in the project:

- display the startup message

### Example 4

```asm
mov bx, KERNAL_OFFSET
mov dh, 2
call disk_load
```

Plain English:

- set the destination address to `4096`
- ask to read `2` sectors
- call the disk loader

Meaning in the project:

- load the kernel from disk into memory

### Example 5

```asm
mov eax, cr0
or eax, 0x1
mov cr0, eax
```

Plain English:

- copy `cr0` into `eax`
- force bit `0` to become `1`
- write the result back to `cr0`

Meaning in the project:

- enable protected mode

### Example 6

```asm
mov edx, VIDEO_MEMORY
mov [edx], ax
```

Plain English:

- point `edx` at VGA text memory
- write the two-byte value in `ax` into that screen cell

Meaning in the project:

- show one character on the display

## Survival Checklist

When assembly feels confusing, ask these questions:

1. Is this line moving a number, or reading from memory?
2. Which register currently holds the important address?
3. Are we in 16-bit mode or 32-bit mode?
4. Is this using BIOS, or direct hardware memory?
5. Is the code calling a function, returning, or jumping away?

If you answer those five questions, most short assembly blocks become readable.

## Best Reading Order For This Repo

1. `Readme.md`
2. `basic asm to survive/README.md`
3. `bootsector/README.md`
4. `bootsector/bootsector_main.asm`
5. `kernal/README.md`
6. `kernal/kernal_entry.asm`
7. `kernal/kernal_entry.c`

## Final Plain-English Summary

This project is doing one simple but important thing:

- start with raw boot code
- load more code from disk
- switch the CPU into a better mode
- run a tiny C kernel
- write directly to the screen

That is the basic skeleton of an operating system boot path.
