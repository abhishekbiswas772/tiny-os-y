#include "keyboard.h"
#include "../ports/ports.h"
#include "../../cpu/isr/isr.h"
#include "../screen/screen.h"
#include "../../kernal/libc/string.h"
#include "../../kernal/libc/function.h"
#include "../../kernal/kernel.h"

/* Extern declarations for GUI manager (avoids clangd relative include bugs) */
extern int gui_active;
extern void gui_handle_keyboard(u8 scancode, char ascii);

#define BACKSPACE  0x0E
#define ENTER      0x1C
#define LSHIFT     0x2A
#define RSHIFT     0x36
#define LSHIFT_REL 0xAA
#define RSHIFT_REL 0xB6
#define SC_MAX     57

static char key_buffer[256];
static int  shift_held = 0;

/* Unshifted (default) — all letters lowercase */
static const char sc_ascii[] = {
    '?', '?', '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '?', '?',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '?', '?', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`', '?', '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm',
    ',', '.', '/', '?', '?', '?', ' '
};

/* Shifted — uppercase letters + shifted symbols */
static const char sc_ascii_shift[] = {
    '?', '?', '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '?', '?',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '?', '?', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    ':', '"', '~', '?', '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M',
    '<', '>', '?', '?', '?', '?', ' '
};

static void keyboard_callback(registers_t *regs) {
    u8 scancode = port_byte_in(0x60);

    /* Track shift keys (release scancodes have bit 7 set) */
    if (scancode == LSHIFT || scancode == RSHIFT) {
        shift_held = 1; return;
    }
    if (scancode == LSHIFT_REL || scancode == RSHIFT_REL) {
        shift_held = 0; return;
    }

    /* Ignore other key releases (bit 7 set) */
    if (scancode & 0x80) return;

    char letter = 0;
    if (scancode <= SC_MAX) {
        letter = shift_held ? sc_ascii_shift[(int)scancode]
                            : sc_ascii[(int)scancode];
    }

    if (gui_active) {
        gui_handle_keyboard(scancode, letter);
        UNUSED(regs);
        return;
    }

    if (scancode > SC_MAX) return;

    if (scancode == BACKSPACE) {
        backspace(key_buffer);
        kprint_backspace();
    } else if (scancode == ENTER) {
        kprint("\n");
        user_input(key_buffer);
        key_buffer[0] = '\0';
    } else {
        char str[2] = {letter, '\0'};
        append(key_buffer, letter);
        kprint(str);
    }
    UNUSED(regs);
}

void init_keyboard() {
    register_interrupt_handler(IRQ1, keyboard_callback);
}
