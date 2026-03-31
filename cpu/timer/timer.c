#include "timer.h"
#include "../isr/isr.h"
#include "../../drivers/ports/ports.h"
#include "../../kernal/libc/function.h"

static u32 tick = 0;

static void timer_callback(registers_t *regs) {
    tick++;
    UNUSED(regs);
}

void init_timer(u32 freq) {
    register_interrupt_handler(IRQ0, timer_callback);

    u32 divisor = 1193180 / freq;
    u8 low  = (u8)(divisor & 0xFF);
    u8 high = (u8)((divisor >> 8) & 0xFF);

    port_byte_out(0x43, 0x36); /* channel 0, lobyte/hibyte, rate generator */
    port_byte_out(0x40, low);
    port_byte_out(0x40, high);
}
