#ifndef VGA_H
#define VGA_H

#include "../../cpu/types.h"

/* ── Screen geometry in Mode 13h ─────────────────────────────────────────── */
#define VGA_WIDTH  320
#define VGA_HEIGHT 200

/* ── Palette colour indices ───────────────────────────────────────────────── */
#define COL_BLACK       0
#define COL_BLUE        1
#define COL_GREEN       2
#define COL_CYAN        3
#define COL_RED         4
#define COL_MAGENTA     5
#define COL_BROWN       6
#define COL_LGRAY       7
#define COL_DGRAY       8
#define COL_LBLUE       9
#define COL_LGREEN     10
#define COL_LCYAN      11
#define COL_LRED       12
#define COL_LMAGENTA   13
#define COL_YELLOW     14
#define COL_WHITE      15

/* Switch video hardware to VGA Mode 13h (320×200, 256 colours).
 * The linear framebuffer is at physical address 0xA0000. */
void vga_init(void);

/* Fill the entire screen with colour. */
void vga_clear(u8 colour);

/* Plot a single pixel. No bounds checking. */
void vga_put_pixel(int x, int y, u8 colour);

/* Read a single pixel. */
u8   vga_get_pixel(int x, int y);

/* Filled axis-aligned rectangle. Clipped to screen. */
void vga_fill_rect(int x, int y, int w, int h, u8 colour);

/* 1-pixel-wide rectangle outline. */
void vga_draw_rect(int x, int y, int w, int h, u8 colour);

/* Horizontal line. */
void vga_hline(int x, int y, int len, u8 colour);

/* Vertical line. */
void vga_vline(int x, int y, int len, u8 colour);

/* Draw an 8×8 character at pixel position (x, y).
 * Only printable ASCII (0x20–0x7E) is supported; others render as space. */
void vga_draw_char(int x, int y, char c, u8 fg, u8 bg);

/* Draw a null-terminated string; wraps at right edge, advances by 8px. */
void vga_draw_string(int x, int y, const char *s, u8 fg, u8 bg);

/* Set DAC palette entry n to (r, g, b) where each is 0–63. */
void vga_set_palette(u8 n, u8 r, u8 g, u8 b);

#endif
