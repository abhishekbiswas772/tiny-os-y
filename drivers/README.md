# Drivers Folder — Complete Beginner Guide

This folder contains low-level drivers.
A driver is code that talks directly to hardware.
There is no OS here, no libraries — just raw code that controls the machine.

This folder currently has two drivers:

| Folder | What it does |
|--------|-------------|
| `ports/` | Read and write bytes to/from hardware ports |
| `screen/` | Print text to the VGA screen |

---

## Why Do We Need Drivers?

In a normal program on your laptop, you call `printf("hello")` and text appears.
The OS handles everything — talking to the graphics card, managing the cursor, etc.

In this kernel there IS no OS.
If you want text on screen, you must:
1. calculate which memory address corresponds to that screen position
2. write the character byte and color byte there yourself

That is what the screen driver does.

And to move the cursor (the blinking underscore), you must:
1. calculate the cursor position
2. send it to the VGA controller chip through hardware ports

That is what the ports driver enables.

---

## Folder: `ports/` — Hardware Port I/O

### What is a hardware port?

A port is a numbered communication channel to a physical hardware chip.
It is completely separate from regular RAM.

Think of it like this:
- RAM is a giant array of boxes you can read/write
- Ports are a separate set of numbered mailboxes wired directly to hardware chips
- When you write to a port, the hardware chip responds

The x86 CPU has 65536 possible port addresses (0 to 65535).
Different hardware devices listen on specific port numbers.

In this project we use:
- port 948 — VGA cursor control register selector
- port 949 — VGA cursor control data register

### File: `ports.h` — The Function Declarations

```c
unsigned char port_byte_in(unsigned short port);
```
Read 1 byte from the hardware port at address `port`.
Returns the byte that the hardware sent back.

```c
unsigned short port_word_in(unsigned short port);
```
Read 2 bytes (a 16-bit word) from a port.

```c
void port_byte_out(unsigned short port, unsigned char data);
```
Send 1 byte `data` to the hardware port at address `port`.

```c
void port_word_out(unsigned short port, unsigned short data);
```
Send 2 bytes to a port.

### File: `ports.c` — The Actual Hardware Access

These functions use inline assembly because C has no way to issue `in` and `out` instructions.

#### `port_byte_in` — reading a byte from a port

```c
unsigned char port_byte_in(unsigned short port) {
    unsigned char result;
    __asm__ volatile ("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}
```

Step by step:

1. `"d"(port)` — the compiler puts the C variable `port` into the `dx` register
2. `in %%dx, %%al` — the CPU reads 1 byte from the port whose number is in `dx`,
   and puts the result into the `al` register
3. `"=a"(result)` — the compiler reads from `al`/`eax` and stores it into `result`
4. `return result` — hand it back to the caller

Why `%%dx` and not just `dx`?
Inside the `__asm__` string, `%` has special meaning (referencing operand numbers like `%0`).
To write a literal `%` for a register name you must double it: `%%dx` means "the register dx."

#### `port_byte_out` — writing a byte to a port

```c
void port_byte_out(unsigned short port, unsigned char data) {
    __asm__ volatile ("out %%al, %%dx" : : "a"(data), "d"(port));
}
```

Step by step:

1. `"a"(data)` — put `data` into the `al` register
2. `"d"(port)` — put `port` into the `dx` register
3. `out %%al, %%dx` — the CPU sends the byte in `al` to the port whose number is in `dx`
4. No output section (empty `:`) because nothing comes back

#### `port_word_in` — reading 2 bytes

```c
unsigned short port_word_in(unsigned short port){
    unsigned short result;
    __asm__ volatile ("in %%dx, %%ax" : "=a"(result) : "d"(port));
    return result;
}
```

Same as `port_byte_in` but uses `ax` (16-bit) instead of `al` (8-bit).
`in %%dx, %%ax` reads 2 bytes instead of 1.

#### The `volatile` keyword

`volatile` tells the C compiler: do NOT optimize this instruction away.

The compiler is allowed to delete code it thinks has no effect.
A write to a hardware port might look pointless to the compiler — no C variable changes.
But sending that byte to the VGA chip has a real hardware effect (like moving the cursor).
`volatile` forces the compiler to always execute the instruction, no matter what.

---

## Folder: `screen/` — VGA Text Screen Driver

### File: `screen.h` — What the Screen Driver Offers

```c
#define VIDEO_ADDRESS 0xb8000   // = 753664 in decimal
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0f     // = 15 — white text, black background
#define RED_ON_WHITE 0xf4       // = 244 — red text, white background

#define REG_SCREEN_CTRL 0x3d4   // = 948 — VGA cursor control port
#define REG_SCREEN_DATA 0x3d5   // = 949 — VGA cursor data port

void clear_screen();
void kprint_at(char *message, int col, int row);
void kprint(char *message);
```

Three public functions:
- `clear_screen` — wipe the screen, reset cursor to top-left
- `kprint_at` — print a string at a specific column and row
- `kprint` — print a string at the current cursor position

### File: `screen.c` — How Printing Actually Works

#### Helper: `get_offset(col, row)`

```c
int get_offset(int col, int row) {
    return 2 * (row * MAX_COLS + col);
}
```

Converts a column and row into a byte offset from the start of VGA memory.

Why multiply by 2?
Each screen cell uses 2 bytes (character + color).
So cell at column 5, row 3:
- cell index = 3 × 80 + 5 = 245
- byte offset = 245 × 2 = 490
- actual memory address = 753664 + 490 = 754154

#### Helper: `get_offset_row(offset)` and `get_offset_col(offset)`

```c
int get_offset_row(int offset) {
    return offset / (2 * MAX_COLS);
}

int get_offset_col(int offset) {
    return (offset - (get_offset_row(offset) * 2 * MAX_COLS)) / 2;
}
```

These do the reverse: given a byte offset, figure out which row and column it corresponds to.

Example with offset 490:
- row = 490 / (2 × 80) = 490 / 160 = 3
- col = (490 - 3 × 160) / 2 = (490 - 480) / 2 = 10 / 2 = 5

So offset 490 = column 5, row 3. ✓

#### `get_cursor_offset` — asking the VGA chip where the cursor is

```c
int get_cursor_offset(){
    port_byte_out(REG_SCREEN_CTRL, 14);
    int offset = port_byte_in(REG_SCREEN_DATA) << 8;
    port_byte_out(REG_SCREEN_CTRL, 15);
    offset += port_byte_in(REG_SCREEN_DATA);
    return offset * 2;
}
```

The VGA controller chip knows where the text cursor is.
You ask it by talking to ports 948 and 949.

Step by step:

1. `port_byte_out(948, 14)` — tell the VGA chip: "I want to read internal register 14"
   (register 14 holds the HIGH byte of the cursor position)
2. `port_byte_in(949)` — read that register → `high_byte`
3. `offset = high_byte << 8` — shift left 8 bits to put it in the high byte position
4. `port_byte_out(948, 15)` — tell the VGA chip: "now I want register 15"
   (register 15 holds the LOW byte of the cursor position)
5. `port_byte_in(949)` → `low_byte`
6. `offset += low_byte` — combine: cursor position in cell units
7. `return offset * 2` — convert to byte offset (multiply by 2 because each cell = 2 bytes)

Why two separate reads?
The cursor position is a 16-bit number (can be up to 80 × 25 = 2000).
The VGA chip stores it as two 8-bit registers (high byte and low byte).
You read them separately and combine with bit shifting.

What is bit shifting?
`high << 8` moves the bits of `high` left by 8 positions.
In decimal terms: `high × 256`.
So if `high = 1` and `low = 50`:
- `offset = 1 × 256 + 50 = 306`
- byte offset = 306 × 2 = 612

#### `set_cursor_offset` — moving the cursor

```c
void set_cursor_offset(int offset) {
    offset /= 2;
    port_byte_out(REG_SCREEN_CTRL, 14);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset >> 8));
    port_byte_out(REG_SCREEN_CTRL, 15);
    port_byte_out(REG_SCREEN_DATA, (unsigned char)(offset & 0xff));
}
```

Reverse of reading: send the cursor position to the VGA chip.

1. `offset /= 2` — convert byte offset back to cell count
2. Tell VGA chip register 14 = high byte: `(offset >> 8)` shifts right 8 bits = `offset / 256`
3. Tell VGA chip register 15 = low byte: `(offset & 0xff)` keeps only the lowest 8 bits
   (0xff = 255 in decimal — AND with 255 keeps only the last 8 bits)

After this, the hardware cursor (blinking underscore) moves to the new position.

#### `print_char` — writing one character to screen

```c
int print_char(char c, int col, int row, char attr) {
    unsigned char *vidmem = (unsigned char*) VIDEO_ADDRESS;
    if (!attr) attr = WHITE_ON_BLACK;
```
Point `vidmem` at VGA memory address 753664.
If no color was specified, use white on black (15).

```c
    if (col >= MAX_COLS || row >= MAX_ROWS) {
        vidmem[2*(MAX_COLS)*(MAX_ROWS)-2] = 'E';
        vidmem[2*(MAX_COLS)*(MAX_ROWS)-1] = RED_ON_WHITE;
        return get_offset(col, row);
    }
```
Error guard: if the column or row is outside screen bounds, write a red 'E'
at the very last cell of the screen.
This is a visible alarm — it means your code tried to print outside the screen.

```c
    int offset;
    if (col >= 0 && row >= 0) offset = get_offset(col, row);
    else offset = get_cursor_offset();
```
If real coordinates were given, calculate the offset.
If col and row are both negative (means "use current cursor"), read the cursor position.

```c
    if (c == '\n') {
        row = get_offset_row(offset);
        offset = get_offset(0, row+1);
    } else {
        vidmem[offset] = c;
        vidmem[offset+1] = attr;
        offset += 2;
    }
```
Handle the newline character specially:
If the character is `\n` (newline), move to column 0 of the next row.
Otherwise, write the character and color attribute to VGA memory, then advance by 2.

```c
    set_cursor_offset(offset);
    return offset;
}
```
Update the hardware cursor and return the new offset.

#### `kprint_at` — print a string at a specific position

```c
void kprint_at(char *message, int col, int row){
    int offset;
    if (col >= 0 && row >= 0)
        offset = get_offset(col, row);
    else {
        offset = get_cursor_offset();
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
    int i = 0;
    while (message[i] != 0) {
        offset = print_char(message[i++], col, row, WHITE_ON_BLACK);
        row = get_offset_row(offset);
        col = get_offset_col(offset);
    }
}
```

If real coordinates given — start there.
If col and row are both -1 — start at the current cursor.

Loop through every character in the string.
Print each one using `print_char`.
After each character, update row and col from the returned offset.
This handles wrapping: after `print_char` the offset might be on a new row.

`message[i] != 0` — the loop ends when it hits the null terminator (value 0).

#### `kprint` — print at current cursor position

```c
void kprint(char *messages) {
    kprint_at(messages, -1, -1);
}
```
Just calls `kprint_at` with -1, -1 which means "use current cursor position."

#### `clear_screen` — wipe the entire screen

```c
void clear_screen() {
    int screen_size = MAX_COLS * MAX_ROWS;   // = 80 × 25 = 2000 cells
    int i;
    char *screen = (char*) VIDEO_ADDRESS;
    for(i = 0; i < screen_size; i++){
        screen[i * 2] = ' ';               // write space character
        screen[i * 2 + 1] = WHITE_ON_BLACK; // write white-on-black color
    }
    set_cursor_offset(get_offset(0, 0));   // move cursor to top-left
}
```

Loop through all 2000 screen cells.
Write a space character and white-on-black color to each.
This makes the whole screen visually blank.
Then move the cursor to position 0,0 (top-left corner).

---

## How the Screen and Ports Work Together

```
kprint_at("Hello", 0, 0)
   |
   v
kprint_at calls print_char for 'H', then 'e', then 'l', 'l', 'o'
   |
   v
print_char calculates byte offset:
   offset = (0 × 80 + 0) × 2 = 0
   |
   v
print_char writes to VGA memory:
   vidmem[0] = 'H' (character)
   vidmem[1] = 15  (white on black)
   |
   v
print_char calls set_cursor_offset(2)
   |
   v
set_cursor_offset calls port_byte_out(948, 14)
   then port_byte_out(949, high byte)
   then port_byte_out(948, 15)
   then port_byte_out(949, low byte)
   |
   v
VGA chip hardware moves the blinking cursor
   |
   v
'H' appears on screen in top-left corner
```

---

## Important Numbers in Decimal

| Number | What it means |
|--------|--------------|
| 753664 | VGA text memory start address |
| 80 | screen width in characters |
| 25 | screen height in rows |
| 2 | bytes per screen cell (char + color) |
| 15 | color code for white text on black background |
| 244 | color code for red text on white background |
| 948 | VGA cursor control port |
| 949 | VGA cursor data port |
| 14 | VGA internal register: cursor position high byte |
| 15 (register) | VGA internal register: cursor position low byte |
| 0 | null terminator — marks end of a string |
| 10 | ASCII code for newline (line feed) |
| 13 | ASCII code for carriage return |
