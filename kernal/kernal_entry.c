#include "../drivers/screen/screen.h"
#include "../cpu/isr/isr.h"
#include "libc/string.h"
#include "libc/mem.h"
#include "kernel.h"
#include "../drivers/disk/ata.h"
#include "kfs/kfs.h"
#include "kfs/vfs.h"
#include "../drivers/mouse/mouse.h"
#include "../drivers/net/net.h"
#include "../drivers/gui/gui.h"

/* 1 if ATA drive and KFS were found/initialised, 0 otherwise */
static int fs_ready = 0;

/* ── Command parser ──────────────────────────────────────────────────────── */

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Split "cmd arg1 arg2..." into cmd and everything after the first space. */
static void parse_cmd(const char *input, char *cmd, char *arg) {
    int i = 0, ci = 0, ai = 0;

    while (input[i] && is_space(input[i])) i++;
    while (input[i] && !is_space(input[i]) && ci < 31) {
        cmd[ci++] = input[i++];
    }
    cmd[ci] = '\0';

    while (input[i] && is_space(input[i])) i++;
    while (input[i] && ai < 255) arg[ai++] = input[i++];
    while (ai > 0 && is_space(arg[ai - 1])) ai--;
    arg[ai] = '\0';
}

/* Split "word rest" — fills first and the rest (used for "write /path data") */
static void split_first(const char *s, char *first, char *rest) {
    int i = 0, fi = 0, ri = 0;
    while (s[i] && is_space(s[i])) i++;
    while (s[i] && !is_space(s[i]) && fi < 127) first[fi++] = s[i++];
    first[fi] = '\0';

    while (s[i] && is_space(s[i])) i++;
    while (s[i] && ri < 255) rest[ri++] = s[i++];
    rest[ri] = '\0';
}

/* ── Kernel entry ────────────────────────────────────────────────────────── */

void kernel_main() {
    clear_screen();
    isr_install();
    irq_install();

    kprint("KernalOS booting...\n");

    /* ── Mouse ────────────────────────────────────────────────────────── */
    init_mouse();
    kprint("[PS2] Mouse initialised\n");

    /* ── Network ──────────────────────────────────────────────────────── */
    net_init();
    if (net_ready) {
        kprint("[NE2K] Network ready  MAC: ");
        int i;
        char hx[3];
        for (i = 0; i < 6; i++) {
            hex_to_ascii(net_mac[i], hx);
            /* hex_to_ascii gives "0x??"; skip the "0x" prefix */
            kprint(hx + 2);
            if (i < 5) kprint(":");
        }
        kprint("\n");
    } else {
        kprint("[NE2K] No card detected\n");
    }

    /* ── File system init ──────────────────────────────────────────────── */
    if (ata_init() == 0) {
        kprint("[ATA] Drive detected\n");
        if (kfs_mount() == 0) {
            kprint("[KFS] Mounted\n");
            fs_ready = 1;
        } else {
            kprint("[KFS] Not formatted. Formatting now...\n");
            kfs_format();
            if (kfs_mount() == 0) {
                kprint("[KFS] Mounted\n");
                fs_ready = 1;
            } else {
                kprint("[KFS] Format failed\n");
            }
        }
    } else {
        kprint("[ATA] No drive detected — FS commands unavailable\n");
        kprint("      (run QEMU with -hda disk.img)\n");
    }

    kprint("\nCommands: ls  cat  touch  mkdir  write  rm  format\n"
           "          gui  mouse  net  arp  PAGE  END\n> ");
}

/* ── Command handler (called by keyboard driver on ENTER) ────────────────── */

void user_input(char *input) {
    char cmd[32];
    char arg[256];
    parse_cmd(input, cmd, arg);

    if (strcmp(cmd, "END") == 0) {
        kprint("Halting.\n");
        __asm__ __volatile__("cli; hlt");

    } else if (strcmp(cmd, "PAGE") == 0) {
        u32 phys;
        u32 page = kmalloc(1000, 1, &phys);
        char s[16];
        hex_to_ascii((int)page, s);  kprint("Page: "); kprint(s);
        hex_to_ascii((int)phys, s);  kprint("  phys: "); kprint(s); kprint("\n");

    /* ── GUI ─────────────────────────────────────────────────────────── */
    } else if (strcmp(cmd, "gui") == 0) {
        gui_init();
        /* kprint output is invisible in Mode 13h — GUI takes over from here */

    /* ── Mouse state ─────────────────────────────────────────────────── */
    } else if (strcmp(cmd, "mouse") == 0) {
        char s[16];
        kprint("Mouse: x=");
        int_to_ascii(mouse_x, s); kprint(s);
        kprint(" y=");
        int_to_ascii(mouse_y, s); kprint(s);
        kprint(" btn=");
        int_to_ascii((int)mouse_buttons, s); kprint(s);
        kprint("\n");

    /* ── Network info ────────────────────────────────────────────────── */
    } else if (strcmp(cmd, "net") == 0) {
        if (!net_ready) {
            kprint("net: no NE2000 card\n");
        } else {
            kprint("NE2000 at 0x300 IRQ9  MAC: ");
            int i;
            char hx[3];
            for (i = 0; i < 6; i++) {
                hex_to_ascii(net_mac[i], hx);
                kprint(hx + 2);
                if (i < 5) kprint(":");
            }
            kprint("\n");
        }

    /* ── ARP probe (sends a gratuitous ARP request) ──────────────────── */
    } else if (strcmp(cmd, "arp") == 0) {
        if (!net_ready) {
            kprint("arp: no NE2000 card\n");
        } else {
            /* Gratuitous ARP: who-has <our IP> tell <our IP>
             * We don't have DHCP so we announce 10.0.2.15 (QEMU default).
             * Frame layout: 14-byte Ethernet header + 28-byte ARP payload. */
            u8 frame[42];
            int i;

            /* Destination: broadcast */
            for (i = 0; i < 6; i++) frame[i] = 0xFF;
            /* Source: our MAC */
            for (i = 0; i < 6; i++) frame[6 + i] = net_mac[i];
            /* EtherType: ARP (0x0806) */
            frame[12] = 0x08; frame[13] = 0x06;

            /* ARP header */
            frame[14] = 0x00; frame[15] = 0x01;  /* HTYPE: Ethernet */
            frame[16] = 0x08; frame[17] = 0x00;  /* PTYPE: IPv4 */
            frame[18] = 6;                         /* HLEN */
            frame[19] = 4;                         /* PLEN */
            frame[20] = 0x00; frame[21] = 0x01;  /* OPER: request */
            /* SHA: our MAC */
            for (i = 0; i < 6; i++) frame[22 + i] = net_mac[i];
            /* SPA: 10.0.2.15 */
            frame[28]=10; frame[29]=0; frame[30]=2; frame[31]=15;
            /* THA: zeros */
            for (i = 0; i < 6; i++) frame[32 + i] = 0x00;
            /* TPA: 10.0.2.15 (gratuitous) */
            frame[38]=10; frame[39]=0; frame[40]=2; frame[41]=15;

            if (net_send(frame, 42) == 0)
                kprint("arp: sent gratuitous ARP for 10.0.2.15\n");
            else
                kprint("arp: send failed\n");
        }

    } else if (strcmp(cmd, "format") == 0) {
        if (!fs_ready && ata_init() != 0) {
            kprint("format: no ATA drive\n");
        } else {
            kprint("Formatting disk...\n");
            kfs_format();
            fs_ready = (kfs_mount() == 0);
            kprint(fs_ready ? "Done.\n" : "Failed.\n");
        }

    } else if (strcmp(cmd, "ls") == 0) {
        if (!fs_ready) { kprint("ls: no filesystem\n"); }
        else           { vfs_ls(arg[0] ? arg : "/"); }

    } else if (strcmp(cmd, "cat") == 0) {
        if (!fs_ready)    { kprint("cat: no filesystem\n"); }
        else if (!arg[0]) { kprint("usage: cat <path>\n"); }
        else              { vfs_cat(arg); }

    } else if (strcmp(cmd, "touch") == 0) {
        if (!fs_ready)    { kprint("touch: no filesystem\n"); }
        else if (!arg[0]) { kprint("usage: touch <path>\n"); }
        else              { vfs_touch(arg); }

    } else if (strcmp(cmd, "mkdir") == 0) {
        if (!fs_ready)    { kprint("mkdir: no filesystem\n"); }
        else if (!arg[0]) { kprint("usage: mkdir <path>\n"); }
        else              { vfs_mkdir(arg); }

    } else if (strcmp(cmd, "rm") == 0) {
        if (!fs_ready)    { kprint("rm: no filesystem\n"); }
        else if (!arg[0]) { kprint("usage: rm <path>\n"); }
        else              { vfs_rm(arg); }

    } else if (strcmp(cmd, "write") == 0) {
        if (!fs_ready) { kprint("write: no filesystem\n"); }
        else if (!arg[0]) { kprint("usage: write <path> <data>\n"); }
        else {
            char path[128], data[256];
            split_first(arg, path, data);
            if (!path[0] || !data[0]) { kprint("usage: write <path> <data>\n"); }
            else                       { vfs_write(path, data); }
        }

    } else if (cmd[0] != '\0') {
        kprint("Unknown: ");
        kprint(cmd);
        kprint("\n");
    }

    kprint("> ");
}
