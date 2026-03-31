#include "../drivers/screen/screen.h"
#include "../cpu/isr/isr.h"
#include "libc/string.h"
#include "libc/mem.h"
#include "kernel.h"

void kernel_main() {
    clear_screen();
    isr_install();
    irq_install();

    kprint("Type something, it will go through the kernel\n"
           "Commands: END  PAGE\n> ");
}

void user_input(char *input) {
    if (strcmp(input, "END") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        __asm__ __volatile__("hlt");
    } else if (strcmp(input, "PAGE") == 0) {
        u32 phys_addr;
        u32 page = kmalloc(1000, 1, &phys_addr);
        char page_str[16] = "";
        hex_to_ascii(page, page_str);
        char phys_str[16] = "";
        hex_to_ascii(phys_addr, phys_str);
        kprint("Page: ");
        kprint(page_str);
        kprint(", phys: ");
        kprint(phys_str);
        kprint("\n");
    }
    kprint("You said: ");
    kprint(input);
    kprint("\n> ");
}
