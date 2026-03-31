#ifndef TIMER_H
#define TIMER_H

#include "../types.h"

void init_timer(u32 freq);
u32 timer_get_ticks(void);

#endif
