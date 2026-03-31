#ifndef NET_H
#define NET_H

#include "../../cpu/types.h"

#define ETH_FRAME_MAX 1514   /* max Ethernet payload (excl. CRC) */

/* Initialise the NE2000 ISA card at I/O base 0x300, IRQ9. */
void net_init(void);

/* Send an Ethernet frame. data must be a complete raw frame (dst, src, type,
 * payload). Pads to minimum 60 bytes if shorter.
 * Returns 0 on success, -1 on error or timeout. */
int net_send(u8 *data, u16 len);

/* Copy the oldest received frame into buf and set *len.
 * Returns 0 on success, -1 if no frame available. */
int net_recv(u8 *buf, u16 *len);

/* Our MAC address — available after net_init(). */
extern u8 net_mac[6];

/* 1 if net_init() found a card, 0 otherwise. */
extern int net_ready;

#endif
