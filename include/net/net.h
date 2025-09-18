#ifndef NET_NET_H
#define NET_NET_H

#include <stdint.h>
#include <stddef.h>

void net_init(void);
void net_poll(void);
void net_send_arp_probe(const uint8_t ip[4]);

#endif /* NET_NET_H */
