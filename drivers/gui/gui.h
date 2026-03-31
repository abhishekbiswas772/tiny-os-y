#ifndef GUI_H
#define GUI_H

#include "../../cpu/types.h"

extern int gui_active;

/* Initialise VGA Mode 13h and draw the initial desktop.
 * Call init_mouse() before gui_init() so the cursor works immediately. */
void gui_init(void);

/* Redraw the mouse cursor at (mouse_x, mouse_y), erasing the previous one.
 * Call from the IRQ12 handler or any convenient place after mouse state
 * has been updated. */
void gui_update_cursor(void);

/* Draw a titled window frame at (x, y) with inner size w×h. */
void gui_draw_window(int x, int y, int w, int h, const char *title);

/* Print a line of text in the desktop "console" area (below the taskbar). */
void gui_print(const char *s);

/* Handle keystrokes routed from the keyboard driver. */
void gui_handle_keyboard(u8 scancode, char ascii);

#endif
