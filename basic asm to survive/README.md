# Basic ASM To Survive — The Complete Beginner Survival Guide

This is not boot code.
This is the guide you read BEFORE touching the boot code.
Think of it as a cheat sheet you keep open on the side.

No hex numbers here unless absolutely unavoidable.
No assumed knowledge.
Treat everything like you are reading it for the first time.

---

## Chapter 1: What Even Is Assembly?

When you write a C or Python program, a tool called a compiler translates your
human-friendly code into machine instructions the CPU can run.

Assembly skips most of that translation.
Assembly IS the machine instructions, just written with human-readable names.

A CPU only does a few things over and over:

- move a number from one place to another
- do math (add, subtract, multiply)
- compare two numbers
- jump to a different instruction
- read from memory
- write to memory

Assembly lets you say exactly that, one instruction at a time.

### Why does this project use assembly?

Because when the machine first powers on, no C runtime, no OS, no nothing exists yet.
The very first code that runs must be hand-crafted assembly that sets up the world
before C can even begin.

---

## Chapter 2: Registers — The CPU's Own Tiny Notepad

Imagine the CPU has a tiny notepad with only a few pages.
Those pages are called registers.

Registers are stored inside the chip itself.
Reading or writing a register is thousands of times faster than touching normal RAM.

### The 16-bit registers (used in real mode — early boot)

| Register | Nickname | Common use in this project |
|----------|----------|---------------------------|
| `ax` | accumulator | math, BIOS call results |
| `bx` | base | pointer to a string or memory address |
| `cx` | counter | loop counts |
| `dx` | data | extra values, disk/port operations |
| `sp` | stack pointer | top of the current stack |
| `bp` | base pointer | helps set up the stack |
| `si` | source index | source address for string ops |
| `di` | destination index | destination address for string ops |

### The 32-bit registers (used in protected mode — after boot switches)

These are the same registers but wider (32 bits instead of 16).
The CPU gains access to them after switching to protected mode.

| Register | Relationship |
|----------|-------------|
| `eax` | 32-bit version of `ax` |
| `ebx` | 32-bit version of `bx` |
| `ecx` | 32-bit version of `cx` |
| `edx` | 32-bit version of `dx` |
| `esp` | 32-bit stack pointer |
| `ebp` | 32-bit base pointer |

The `e` just means "extended."
Think of it as upgrading from a small notebook page to a bigger one.

### The segment registers (special memory selectors)

| Register | Name |
|----------|------|
| `cs` | code segment |
| `ds` | data segment |
| `ss` | stack segment |
| `es`, `fs`, `gs` | extra segments |

You mostly don't touch these manually in C.
But during the boot mode switch they are set up manually.

### The control register

| Register | Purpose |
|----------|---------|
| `cr0` | Control register 0 — contains critical CPU mode bits |

One bit in `cr0` controls whether the CPU is in 16-bit real mode or 32-bit protected mode.

---

## Chapter 3: Memory — The Huge Numbered List

RAM is just a very long row of numbered boxes.
Each box holds exactly 1 byte (a number from 0 to 255).
Each box has an address — a number saying where it lives.

```
Address:   0    1    2    3    4    5    6 ...
Content: [72] [101] [108] [108] [111] [0] [?] ...
```

That example above stores the word "Hello" followed by a zero.
The zero marks the end of the string. This is called a null terminator.

### Important memory addresses in this project (in decimal)

| Decimal address | What lives there |
|----------------|-----------------|
| 31744 | Where BIOS loads the boot sector |
| 36864 | Where the early real-mode stack begins |
| 4096 | Where the kernel is loaded |
| 589824 | Where the 32-bit protected-mode stack lives |
| 753664 | VGA text screen memory — writing here prints to screen |

### Square brackets mean "go to this address and read/write there"

```asm
mov bx, 4096
```
This puts the NUMBER 4096 into `bx`.

```asm
mov al, [bx]
```
This goes to memory address 4096, reads 1 byte, puts it in `al`.

```asm
mov [bx], al
```
This writes the byte in `al` TO memory address 4096.

The brackets are the difference between the address and what is AT the address.
Like the difference between a house number and what is inside the house.

---

## Chapter 4: The Stack — A Pile of Paper

The stack is a region of memory that works like a pile of paper.
You put things on top (push) and take from the top (pop).

The last thing you put on comes off first.
This is called LIFO: Last In, First Out.

```
Before push:          After push 42:        After push 99:
                      +--------+            +--------+
                      |   42   | <-- top    |   99   | <-- top
                      +--------+            +--------+
                                            |   42   |
                                            +--------+
```

After two pops:
- first pop gives you 99
- second pop gives you 42

### Why the CPU cares about the stack

When you call a function, the CPU needs to remember where to come back to.
It puts that return address on the stack automatically.
When the function finishes and hits `ret`, the CPU pops that address and jumps back.

So if the stack is broken, function calls crash.
That is why one of the very first things the bootloader does is set up a valid stack.

### Stack instructions

```asm
push ax          ; copy value in ax onto the top of the stack
pop  ax          ; take the top value off the stack, put it in ax
pusha            ; push ALL general-purpose registers at once (saves your work)
popa             ; pop ALL general-purpose registers at once (restores your work)
```

`pusha` and `popa` are used in this project at the start and end of every helper function.
This way the function doesn't accidentally destroy register values the caller was using.

### The stack pointer register

`sp` (or `esp` in 32-bit mode) always points to the top of the stack.
On x86, the stack grows downward in memory.
Each push decreases `esp`. Each pop increases it.

```
High address
+------------+
|            |  <-- esp starts here (initial stack base)
+------------+
|    99      |  <-- esp after one push
+------------+
|    42      |  <-- esp after two pushes
+------------+
Low address
```

---

## Chapter 5: Instructions — The Actual Words of Assembly

### Moving data

```asm
mov ax, 5        ; put the number 5 into ax
mov bx, ax       ; copy what's in ax into bx (both become 5)
mov [100], ax    ; write the value of ax to memory address 100
mov ax, [100]    ; read from memory address 100 into ax
```

### Math

```asm
add ax, 3        ; ax = ax + 3
sub ax, 1        ; ax = ax - 1
or  ax, 1        ; set bit 0 of ax to 1 (bitwise OR)
and ax, 3        ; keep only bits that are set in both ax and 3
```

### Comparing

```asm
cmp ax, 0        ; compare ax with 0 (does NOT change ax, just sets internal CPU flags)
```

After `cmp`, you use a jump instruction to act on the result.

### Jumping

```asm
jmp label        ; always jump to label
je  label        ; jump to label IF last compare was equal
jne label        ; jump to label IF last compare was NOT equal
jc  label        ; jump if carry flag is set (used for disk error checking)
jle label        ; jump if less than or equal
```

### Calling functions

```asm
call my_function   ; push return address, jump to my_function
ret                ; pop return address, jump back to caller
```

### The infinite halt

```asm
jmp $            ; jump to the current address — run this line forever
```

This is used when there is nothing else to do and you don't want the CPU running garbage.

---

## Chapter 6: BIOS Interrupts — Asking the Firmware for Help

In 16-bit real mode, the BIOS (the chip's built-in firmware) provides helper functions.
You activate them using an `int` instruction followed by a number.

Think of `int` as "call this BIOS service number."

Before calling `int`, you put arguments in specific registers.

### Printing a character on screen — interrupt 16

The source code uses `int 0x10`.
Interrupt 16 in decimal.

To print a character:

```asm
mov ah, 14       ; service 14 = "write character to screen"
mov al, 72       ; ASCII code 72 = letter 'H'
int 16           ; ask BIOS to do it
```

The register `ah` holds the service number (14 = print one character).
The register `al` holds the character (ASCII code).

After the interrupt the BIOS prints it and advances the cursor.

### Reading from disk — interrupt 19

The source code uses `int 0x13`.
Interrupt 19 in decimal.

To read sectors:

```asm
mov ah, 2        ; service 2 = "read sectors from disk"
mov al, 15       ; number of sectors to read
mov ch, 0        ; cylinder 0
mov cl, 2        ; start reading from sector 2 (sector 1 is the boot sector itself)
mov dh, 0        ; head 0
; dl already has the boot drive number
; es:bx points to where to put the data
int 19
```

If `al` (sectors actually read) doesn't match what you asked for, something went wrong.
The `jc` instruction checks the carry flag — if it's set, there was a disk error.

---

## Chapter 7: Labels — Names for Addresses

In assembly you can name any memory location.
That name is called a label.

```asm
start:
    mov ax, 5
    jmp end

middle:
    mov ax, 9

end:
    jmp $
```

The assembler replaces each label with the actual memory address when it builds the binary.
So `jmp end` becomes "jump to address 15" or whatever the real address turns out to be.

Labels are how you write function names in assembly.

---

## Chapter 8: Defining Data in Assembly

### `db` — define byte (1 byte)

```asm
MY_VALUE db 42        ; store the number 42 at this location
MY_STRING db 'Hello', 0   ; store the string "Hello" followed by a zero terminator
```

### `dw` — define word (2 bytes)

```asm
MY_WORD dw 1234       ; store the number 1234 in 2 bytes
```

### `dd` — define double word (4 bytes)

```asm
MY_DWORD dd 0         ; store 4 zero bytes
```

### `equ` — define a constant (not stored in memory, just a name for a number)

```asm
KERNEL_LOAD_ADDR equ 4096    ; every time we write KERNEL_LOAD_ADDR the assembler uses 4096
```

---

## Chapter 9: The `[bits 16]` and `[bits 32]` Directives

These are instructions to the NASM assembler, not to the CPU.

```asm
[bits 16]
```
This tells NASM: "generate 16-bit instructions from here."

```asm
[bits 32]
```
This tells NASM: "generate 32-bit instructions from here."

Why does it matter?

Because the same instruction can produce different machine code depending on the mode.
If you write 32-bit instructions but the CPU is still in 16-bit mode, the CPU misreads them
and everything crashes.

In this project:
- boot sector starts with `[bits 16]` because the CPU boots in 16-bit real mode
- after the mode switch, code uses `[bits 32]`

---

## Chapter 10: The `[org]` Directive

```asm
[org 31744]
```

This tells NASM: "assume this code will be loaded at memory address 31744."

Why does this matter?

When you write a label like `MSG_REAL_MODE`, the assembler calculates its address.
If it assumes the code starts at 0 but the code actually runs from 31744, all addresses
will be wrong by 31744 and the program will crash.

`org` corrects the math so every label gets its real runtime address.

The boot sector uses `[org 31744]` because the BIOS always loads the boot sector to
that address before running it.

---

## Chapter 11: The `times` Directive — Padding to Exact Size

```asm
times 510 - ($ - $$) db 0
```

This looks scary but the idea is simple.

- `$` means "current address"
- `$$` means "address where this section started"
- `$ - $$` means "how many bytes we've written so far"
- `510 - ($ - $$)` means "how many bytes we still need to fill up to reach 510"
- `db 0` means fill with zero bytes

The boot sector must be exactly 512 bytes.
The last 2 bytes must be the boot signature (43605).
So the first 510 bytes are the actual code plus zero padding.

This line does the padding automatically no matter how much code you wrote.

---

## Chapter 12: The Boot Signature

```asm
dw 43605
```

The source writes this as `0xAA55` which is 43605 in decimal.

The BIOS checks the very last 2 bytes of the first 512-byte sector.
If those 2 bytes equal 43605, the BIOS considers it a valid boot sector and runs it.
If those 2 bytes are anything else, the BIOS says "not bootable" and stops.

So the boot signature is basically a secret knock that tells the BIOS:
"yes, this disk is actually bootable, please run this code."

---

## Chapter 13: Real Mode vs Protected Mode

This is the most important mode concept in the project.

### Real Mode (16-bit, used at first boot)

- CPU starts here automatically after power-on
- registers are 16-bit
- can only address a small amount of memory efficiently
- BIOS interrupts work here
- it is old, limited, and from the 1980s

Think of real mode as the CPU acting like a very old simple computer.

### Protected Mode (32-bit, switched into by the bootloader)

- must be switched into manually
- registers become 32-bit
- can address up to 4 gigabytes of memory
- BIOS interrupts NO LONGER work here
- much more powerful foundation for an OS

Once you switch to protected mode, you can't easily go back.
The bootloader switches to protected mode right before calling the kernel.

### How the switch happens (simplified)

1. build a GDT (memory layout table for the CPU)
2. tell the CPU where the GDT lives (`lgdt` instruction)
3. set one bit in the `cr0` register to 1
4. do a far jump to flush the CPU pipeline
5. update all segment registers to point into the new GDT
6. set up a new 32-bit stack

After that, the CPU is fully in 32-bit protected mode.

---

## Chapter 14: VGA Text Memory — Writing Directly to the Screen

In protected mode (and also in real mode), there is a special region of memory
starting at address 753664.

If you write bytes to that address, they appear on screen.

The screen is 80 characters wide and 25 rows tall.
That gives 2000 character positions.
Each position uses 2 bytes:

```
byte 0: the character (ASCII code)
byte 1: the color attribute
```

### Color attribute

Color byte 15 means white character on black background.
Color byte 244 means red character on white background.

So to print 'H' in white on black at position 0:

```
memory address 753664 = 72     (ASCII code for 'H')
memory address 753665 = 15     (white on black)
```

To print at position 1 (next character), you use addresses 753666 and 753667.
Each position is 2 bytes apart.

To get the memory address of column `c`, row `r`:

```
address = 753664 + (row * 80 + col) * 2
```

---

## Chapter 15: Inline Assembly Inside C — `__asm__ volatile`

Sometimes C needs to run a single CPU instruction that C itself has no way to express.
For example, reading from or writing to a hardware port.

C has no keyword for that.
So you embed raw assembly directly inside C using `__asm__`.

### Basic syntax

```c
__asm__ volatile ("asm instruction here" : outputs : inputs);
```

- `volatile` means "don't optimize this away — it really must run"
- `: outputs :` tells the compiler which C variable receives a result from the assembly
- `: inputs :` tells the compiler which C variables feed into the assembly

### The constraint letters

| Letter | Meaning |
|--------|---------|
| `"a"` | use the `eax`/`ax`/`al` register |
| `"d"` | use the `edx`/`dx`/`dl` register |
| `"=a"` | write the result back to this C variable using `eax`/`al` |

### Example 1: Reading a byte from a hardware port

```c
unsigned char port_byte_in(unsigned short port) {
    unsigned char result;
    __asm__ volatile ("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}
```

Step by step:

1. `"d"(port)` — put the C variable `port` into the `dx` register
2. `in %%dx, %%al` — run the `in` instruction: read 1 byte from hardware port number in `dx`, result goes into `al`
3. `"=a"(result)` — take whatever is in `al`/`eax` now and store it into the C variable `result`

Why `%%` instead of `%`?
Inside `__asm__` strings, `%` by itself has a special meaning (referencing operand numbers).
To write a literal `%` for a register name you use `%%`.

### Example 2: Writing a byte to a hardware port

```c
void port_byte_out(unsigned short port, unsigned char data) {
    __asm__ volatile ("out %%al, %%dx" : : "a"(data), "d"(port));
}
```

Step by step:

1. `"a"(data)` — put `data` into `al`
2. `"d"(port)` — put `port` into `dx`
3. `out %%al, %%dx` — run the `out` instruction: send the byte in `al` to the port number in `dx`
4. No output operands (empty `:`) because nothing comes back

### What are hardware ports?

Ports are a parallel communication channel the CPU uses to talk to hardware devices.
Think of them as numbered mailboxes — each hardware device listens on specific port numbers.

In this project, the VGA cursor controller listens on two ports:

- port 948 — the control port (you tell it which register you want to access)
- port 949 — the data port (you read or write the actual value)

To ask the cursor controller for the cursor position:

```c
port_byte_out(948, 14);          // tell it: "I want cursor high byte (register 14)"
int high = port_byte_in(949);    // read the high byte of the cursor position

port_byte_out(948, 15);          // tell it: "I want cursor low byte (register 15)"
int low = port_byte_in(949);     // read the low byte

int cursor_position = (high << 8) + low;   // combine into one number
```

The result is the cursor's position counted in number of characters from the top-left.
Multiply by 2 to get the byte offset into VGA memory.

### Why `volatile` matters

The compiler is allowed to remove code it thinks does nothing.
A write to a port looks like a pointless memory write to the compiler.
`volatile` tells the compiler: "this instruction has a side effect that matters — DO NOT remove it."

Without `volatile`, the compiler might silently delete your port writes and your kernel breaks.

---

## Chapter 16: The `in` and `out` CPU Instructions

These are real x86 instructions for talking to hardware.

| Instruction | Direction | Effect |
|-------------|-----------|--------|
| `in al, dx` | hardware → CPU | read 1 byte from the port whose number is in `dx`, put it in `al` |
| `out al, dx` | CPU → hardware | send the byte in `al` to the port whose number is in `dx` |

These work differently from normal memory reads/writes.
Normal memory goes through RAM.
Port I/O goes through a separate bus to physical hardware chips on the motherboard.

---

## Chapter 17: Calling C From Assembly

The boot sector is written in assembly.
The kernel is written in C.
How does the boot code hand control to C?

1. The kernel binary is loaded into memory at address 4096.
2. The very first bytes at address 4096 are from `kernal_entry.asm`.
3. That assembly file contains one job: call the C function `main`.

```asm
[bits 32]
[extern main]       ; tell the assembler: main exists somewhere else (in C)

call main           ; call it like any other function
jmp $               ; if main ever returns, stop here
```

The `extern` keyword tells the assembler that `main` is defined in another file.
The linker connects the assembly `call main` to the real C function during the build.

This is the bridge between assembly land and C land.

---

## Chapter 18: Freestanding C — C Without an Operating System

Normal C on your laptop has a huge amount of support underneath it:

- an operating system managing processes and memory
- a C runtime library that runs before `main` and sets things up
- `printf`, `malloc`, and hundreds of standard library functions

A freestanding kernel has NONE of that.

When this project compiles C with `-ffreestanding`, it means:

- there is no standard library
- there is no automatic setup before `main`
- there is no `printf`
- there is no `malloc`
- if you want to print, you write directly to VGA memory yourself

This is why the kernel has its own `kprint` function.
It is a handmade replacement for `printf` that writes directly to VGA memory
without needing any OS or library.

---

## Chapter 19: The Full Boot Flow in Plain English

```
1. Power on
2. BIOS chip runs — does hardware checks
3. BIOS finds the first bootable disk
4. BIOS reads first 512 bytes into memory address 31744
5. BIOS checks the last 2 bytes equal 43605 — if yes, treat it as bootable
6. BIOS jumps to address 31744 — our boot sector starts
7. Boot sector saves which disk it booted from
8. Boot sector sets up a stack at 36864
9. Boot sector prints "Starting in 16-bit real mode" using BIOS interrupt 16
10. Boot sector prints "Loading kernel into memory" and reads 15 sectors from disk
    into memory address 4096 using BIOS interrupt 19
11. Boot sector builds the GDT — tells CPU how memory is laid out in protected mode
12. Boot sector runs switch_to_pm — disables interrupts, loads GDT,
    flips the protected mode bit in cr0, does far jump
13. CPU is now in 32-bit protected mode
14. Boot sector updates all segment registers to use the new GDT
15. Boot sector moves the stack to 589824
16. Boot sector prints "Landed in 32-bit protected mode" by writing to VGA memory 753664
17. Boot sector calls the kernel at memory address 4096
18. kernal_entry.asm runs — calls main()
19. kernal_entry.c runs — clear_screen(), then kprint_at() and kprint() calls
20. Text appears on screen
```

---

## Chapter 20: Survival Checklist — When Assembly Confuses You

When you see an assembly instruction you don't understand, answer these in order:

1. Is this `mov`, `push`, `pop`, `call`, `ret`, `jmp`, `cmp`, or math?
   — If yes, look it up in Chapter 5
2. Are there square brackets?
   — Yes → it's reading or writing MEMORY at that address
   — No → it's using the value directly
3. Are we in `[bits 16]` or `[bits 32]`?
   — Affects which registers and modes are active
4. Is this a BIOS call (`int 16` or `int 19`)?
   — Real mode only, look up Chapter 6
5. Is this touching `cr0`?
   — This is the protected mode switch (Chapter 13)
6. Is this using `in` or `out`?
   — Hardware port I/O (Chapter 16)
7. Is this `__asm__ volatile` in a C file?
   — Inline assembly, look up Chapter 15

If you still don't understand a line, search the instruction name + "x86" online.
x86 instructions are very well documented.

---

## Quick Reference Card

```
mov dst, src       copy src into dst
add dst, val       dst = dst + val
sub dst, val       dst = dst - val
or  dst, val       dst = dst | val  (set bits)
and dst, val       dst = dst & val  (clear bits)
cmp a, b           compare a and b (sets CPU flags, doesn't change a or b)
je  label          jump if equal (after cmp)
jne label          jump if not equal
jmp label          always jump
jmp $              infinite loop at this spot
call label         push return address, jump to label
ret                pop return address, jump back
push val           put val on the stack
pop  dst           take top of stack into dst
pusha              save all general registers
popa               restore all general registers
int  N             call BIOS service number N
in   al, dx        read 1 byte from hardware port (port number in dx)
out  al, dx        write 1 byte to hardware port (port number in dx)
[bits 16]          assembler: generate 16-bit code
[bits 32]          assembler: generate 32-bit code
[org ADDR]         assembler: assume code starts at ADDR
db VALUE           store 1 byte in the binary
dw VALUE           store 2 bytes in the binary
dd VALUE           store 4 bytes in the binary
NAME equ VALUE     define a constant (no memory used)
times N db 0       repeat N zero bytes
```
