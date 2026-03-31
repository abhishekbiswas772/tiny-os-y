#ifndef MOUSE_H
#define MOUSE_H

#include "../../cpu/types.h"

/* Initialise the PS/2 auxiliary (mouse) port and register IRQ12 handler. */
void init_mouse(void);

/* Current mouse state — updated by IRQ12 handler.
 * Coordinates are clamped to [0, MOUSE_X_MAX] x [0, MOUSE_Y_MAX]. */
extern int mouse_x;
extern int mouse_y;
extern u8  mouse_buttons;  /* bit0=left, bit1=right, bit2=middle */

#define MOUSE_X_MAX 319
#define MOUSE_Y_MAX 199

/* Optional callback fired after every packet — set by gui_init(). */
void mouse_set_move_callback(void (*cb)(void));

#endif
