#include "mouse.h"
#include "../ports/ports.h"
#include "../../cpu/isr/isr.h"
#include "../../kernal/libc/function.h"

#define PS2_DATA 0x60
#define PS2_CMD  0x64

int mouse_x       = 160;
int mouse_y       = 100;
u8  mouse_buttons = 0;

static u8    mouse_cycle = 0;
static u8    mouse_byte[3];
static void (*move_cb)(void) = 0;

void mouse_set_move_callback(void (*cb)(void)) { move_cb = cb; }

/* ── PS/2 controller helpers ─────────────────────────────────────────────── */

/* Wait until the input buffer is empty (safe to write). */
static void ps2_wait_write(void) {
    int i;
    for (i = 0; i < 100000; i++)
        if (!(port_byte_in(PS2_CMD) & 0x02)) return;
}

/* Wait until the output buffer is full (data ready to read). */
static void ps2_wait_read(void) {
    int i;
    for (i = 0; i < 100000; i++)
        if (port_byte_in(PS2_CMD) & 0x01) return;
}

/* Send a byte directly to the mouse (routed via 0xD4 controller command). */
static void mouse_write(u8 data) {
    ps2_wait_write();
    port_byte_out(PS2_CMD, 0xD4);   /* next byte to auxiliary device */
    ps2_wait_write();
    port_byte_out(PS2_DATA, data);
}

static u8 mouse_read(void) {
    ps2_wait_read();
    return port_byte_in(PS2_DATA);
}

/* ── IRQ12 handler ────────────────────────────────────────────────────────── */

static void mouse_callback(registers_t *regs) {
    mouse_byte[mouse_cycle++] = port_byte_in(PS2_DATA);
    if (mouse_cycle < 3) return;   /* wait for a complete 3-byte packet */
    mouse_cycle = 0;

    u8  status = mouse_byte[0];
    int dx     = (int)mouse_byte[1] - ((status & 0x10) ? 256 : 0);
    int dy     = (int)mouse_byte[2] - ((status & 0x20) ? 256 : 0);

    mouse_x += dx;
    mouse_y -= dy;   /* screen Y increases downward; mouse Y increases upward */

    if (mouse_x < 0)            mouse_x = 0;
    if (mouse_x > MOUSE_X_MAX)  mouse_x = MOUSE_X_MAX;
    if (mouse_y < 0)            mouse_y = 0;
    if (mouse_y > MOUSE_Y_MAX)  mouse_y = MOUSE_Y_MAX;

    mouse_buttons = status & 0x07;
    if (move_cb) move_cb();
    UNUSED(regs);
}

/* ── Public init ──────────────────────────────────────────────────────────── */

void init_mouse(void) {
    /* Enable the auxiliary (mouse) port */
    ps2_wait_write();
    port_byte_out(PS2_CMD, 0xA8);

    /* Enable auxiliary interrupt in the PS/2 controller command byte */
    ps2_wait_write();
    port_byte_out(PS2_CMD, 0x20);        /* read current command byte */
    u8 status = mouse_read() | 0x02;    /* set bit 1 = aux IRQ enable */
    ps2_wait_write();
    port_byte_out(PS2_CMD, 0x60);        /* write command byte */
    ps2_wait_write();
    port_byte_out(PS2_DATA, status);

    /* Tell the mouse to use default settings, then enable streaming */
    mouse_write(0xF6);  mouse_read();   /* Set Defaults — ACK */
    mouse_write(0xF4);  mouse_read();   /* Enable — ACK */

    register_interrupt_handler(IRQ12, mouse_callback);
}
