#include "net.h"
#include "../ports/ports.h"
#include "../../cpu/isr/isr.h"
#include "../../kernal/libc/function.h"

/* ── NE2000 register map (base 0x300) ────────────────────────────────────── */
#define NE_BASE   0x300

/* Page 0 (R/W) */
#define NE_CR     (NE_BASE + 0x00)  /* Command register */
#define NE_PSTART (NE_BASE + 0x01)  /* Page start  (W)  */
#define NE_PSTOP  (NE_BASE + 0x02)  /* Page stop   (W)  */
#define NE_BNRY   (NE_BASE + 0x03)  /* Boundary pointer */
#define NE_TPSR   (NE_BASE + 0x04)  /* TX page start (W) */
#define NE_TBCR0  (NE_BASE + 0x05)  /* TX byte count lo (W) */
#define NE_TBCR1  (NE_BASE + 0x06)  /* TX byte count hi (W) */
#define NE_ISR    (NE_BASE + 0x07)  /* Interrupt status */
#define NE_RSAR0  (NE_BASE + 0x08)  /* Remote DMA addr lo (W) */
#define NE_RSAR1  (NE_BASE + 0x09)  /* Remote DMA addr hi (W) */
#define NE_RBCR0  (NE_BASE + 0x0A)  /* Remote DMA byte count lo (W) */
#define NE_RBCR1  (NE_BASE + 0x0B)  /* Remote DMA byte count hi (W) */
#define NE_RCR    (NE_BASE + 0x0C)  /* Receive config (W) */
#define NE_TCR    (NE_BASE + 0x0D)  /* Transmit config (W) */
#define NE_DCR    (NE_BASE + 0x0E)  /* Data config (W) */
#define NE_IMR    (NE_BASE + 0x0F)  /* Interrupt mask (W) */
#define NE_DATA   (NE_BASE + 0x10)  /* Remote DMA data port */
#define NE_RESET  (NE_BASE + 0x1F)  /* Reset (R triggers reset) */

/* Page 1 registers (select via CR bits 7:6 = 01) */
#define NE_PAR0   (NE_BASE + 0x01)  /* Physical address byte 0 */
#define NE_CURR   (NE_BASE + 0x07)  /* Current RX page */

/* CR bits */
#define CR_STOP  0x01
#define CR_START 0x02
#define CR_TXP   0x04   /* transmit packet */
#define CR_RD0   0x08   /* remote DMA: read  */
#define CR_RD1   0x10   /* remote DMA: write */
#define CR_RD2   0x20   /* remote DMA: abort/complete */
#define CR_PAGE0 0x00
#define CR_PAGE1 0x40
#define CR_PAGE2 0x80

/* ISR bits */
#define ISR_PRX  0x01   /* packet received */
#define ISR_PTX  0x02   /* packet transmitted */
#define ISR_RXE  0x04   /* receive error */
#define ISR_TXE  0x08   /* transmit error */
#define ISR_RST  0x80   /* reset complete */

/* Buffer layout (256-byte NE2000 pages in card RAM):
 *   0x00-0x3F : PROM / unused (not used for data)
 *   0x40-0x45 : TX staging buffer  (6 × 256 = 1536 bytes — room for max frame)
 *   0x46-0x7F : RX ring buffer     (58 × 256 = 14 848 bytes)               */
#define TX_PAGE_START  0x40
#define RX_PAGE_START  0x46
#define RX_PAGE_STOP   0x80

u8  net_mac[6];
int net_ready = 0;

static u8  rx_next_page;
static u8  rx_buf[ETH_FRAME_MAX + 4];
static u16 rx_len;
static int rx_ready;

/* ── Remote DMA helpers ───────────────────────────────────────────────────── */

static void ne_wait_rdc(void) {
    int t;
    for (t = 0; t < 0x10000; t++)
        if (port_byte_in(NE_ISR) & 0x40) break;
    port_byte_out(NE_ISR, 0x40);   /* clear RDC bit */
}

static void ne_write_mem(u16 dest, const u8 *data, u16 len) {
    port_byte_out(NE_RSAR0, (u8)(dest));
    port_byte_out(NE_RSAR1, (u8)(dest >> 8));
    port_byte_out(NE_RBCR0, (u8)(len));
    port_byte_out(NE_RBCR1, (u8)(len >> 8));
    port_byte_out(NE_CR,    CR_START | CR_RD1);   /* remote write */
    u16 i;
    for (i = 0; i < len; i++)
        port_byte_out(NE_DATA, data[i]);
    ne_wait_rdc();
}

static void ne_read_mem(u16 src, u8 *buf, u16 len) {
    port_byte_out(NE_RSAR0, (u8)(src));
    port_byte_out(NE_RSAR1, (u8)(src >> 8));
    port_byte_out(NE_RBCR0, (u8)(len));
    port_byte_out(NE_RBCR1, (u8)(len >> 8));
    port_byte_out(NE_CR,    CR_START | CR_RD0);   /* remote read */
    u16 i;
    for (i = 0; i < len; i++)
        buf[i] = port_byte_in(NE_DATA);
    ne_wait_rdc();
}

/* ── IRQ9 handler ─────────────────────────────────────────────────────────── */

static void net_callback(registers_t *regs) {
    u8 isr = port_byte_in(NE_ISR);
    port_byte_out(NE_ISR, isr);   /* acknowledge all */

    if (isr & ISR_PRX) {
        /* Drain the RX ring until CURR == rx_next_page */
        port_byte_out(NE_CR, CR_START | CR_PAGE1 | CR_RD2);
        u8 curr = port_byte_in(NE_CURR);
        port_byte_out(NE_CR, CR_START | CR_PAGE0 | CR_RD2);

        while (rx_next_page != curr) {
            /* Read the 4-byte NE2000 receive header prepended to each frame */
            u16 hdr_addr = (u16)rx_next_page << 8;
            u8  hdr[4];
            ne_read_mem(hdr_addr, hdr, 4);

            u8  next    = hdr[1];
            u16 pkt_len = (u16)hdr[2] | ((u16)hdr[3] << 8);

            if (pkt_len > 4 && pkt_len <= (u16)(ETH_FRAME_MAX + 4) && !rx_ready) {
                u16 data_len = pkt_len - 4;
                ne_read_mem(hdr_addr + 4, rx_buf, data_len);
                rx_len   = data_len;
                rx_ready = 1;
            }

            /* Advance boundary — must stay one page behind CURR */
            rx_next_page = next;
            u8 bnry = (next == RX_PAGE_START) ? (RX_PAGE_STOP - 1) : (next - 1);
            port_byte_out(NE_BNRY, bnry);

            /* Re-read CURR to catch burst arrivals */
            port_byte_out(NE_CR, CR_START | CR_PAGE1 | CR_RD2);
            curr = port_byte_in(NE_CURR);
            port_byte_out(NE_CR, CR_START | CR_PAGE0 | CR_RD2);
        }
    }
    UNUSED(regs);
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void net_init(void) {
    /* Hardware reset */
    port_byte_in(NE_RESET);
    port_byte_out(NE_RESET, 0);
    int t;
    for (t = 0; t < 0x10000; t++)
        if (port_byte_in(NE_ISR) & ISR_RST) break;

    if (!(port_byte_in(NE_ISR) & ISR_RST))
        return;   /* no card present */

    /* Stop, page 0, abort DMA */
    port_byte_out(NE_CR,    CR_STOP | CR_RD2);
    port_byte_out(NE_DCR,   0x49);   /* 8-bit DMA, FIFO threshold = 8 bytes */
    port_byte_out(NE_RBCR0, 0);
    port_byte_out(NE_RBCR1, 0);
    port_byte_out(NE_RCR,   0x20);   /* monitor mode during init */
    port_byte_out(NE_TCR,   0x02);   /* internal loopback */
    port_byte_out(NE_PSTART, RX_PAGE_START);
    port_byte_out(NE_PSTOP,  RX_PAGE_STOP);
    port_byte_out(NE_BNRY,   RX_PAGE_START);
    port_byte_out(NE_ISR,   0xFF);   /* clear all pending interrupts */
    port_byte_out(NE_IMR,   0x00);   /* mask everything for now */

    /* Read the MAC from the station address PROM (first 12 bytes of card RAM;
     * each byte is duplicated so stride is 2). */
    u8 prom[12];
    ne_read_mem(0x0000, prom, 12);
    int i;
    for (i = 0; i < 6; i++)
        net_mac[i] = prom[i * 2];

    /* Page 1: write physical address and initialise current-page pointer */
    port_byte_out(NE_CR, CR_STOP | CR_PAGE1 | CR_RD2);
    for (i = 0; i < 6; i++)
        port_byte_out(NE_PAR0 + i, net_mac[i]);
    port_byte_out(NE_CURR, RX_PAGE_START);
    for (i = 0; i < 8; i++)
        port_byte_out(NE_BASE + 0x08 + i, 0xFF);  /* accept all multicast */

    /* Back to page 0, start normally */
    port_byte_out(NE_CR,  CR_START | CR_PAGE0 | CR_RD2);
    port_byte_out(NE_TCR, 0x00);   /* normal transmit */
    port_byte_out(NE_RCR, 0x04);   /* accept broadcast + directed */
    port_byte_out(NE_ISR, 0xFF);
    port_byte_out(NE_IMR, ISR_PRX | ISR_PTX | ISR_RXE | ISR_TXE);

    rx_next_page = RX_PAGE_START;
    rx_ready     = 0;
    net_ready    = 1;

    register_interrupt_handler(IRQ9, net_callback);
}

int net_send(u8 *data, u16 len) {
    if (!net_ready)    return -1;
    if (len > 1514)    return -1;
    if (len < 60)      len = 60;   /* pad to Ethernet minimum */

    ne_write_mem((u16)TX_PAGE_START << 8, data, len);

    port_byte_out(NE_TPSR,  TX_PAGE_START);
    port_byte_out(NE_TBCR0, (u8)(len));
    port_byte_out(NE_TBCR1, (u8)(len >> 8));
    port_byte_out(NE_CR,    CR_START | CR_TXP | CR_RD2);

    int t;
    for (t = 0; t < 0x10000; t++) {
        u8 isr = port_byte_in(NE_ISR);
        if (isr & (ISR_PTX | ISR_TXE)) {
            port_byte_out(NE_ISR, ISR_PTX | ISR_TXE);
            return (isr & ISR_TXE) ? -1 : 0;
        }
    }
    return -1;  /* timeout */
}

int net_recv(u8 *buf, u16 *len) {
    if (!rx_ready) return -1;
    *len = rx_len;
    u16 i;
    for (i = 0; i < rx_len; i++)
        buf[i] = rx_buf[i];
    rx_ready = 0;
    return 0;
}
