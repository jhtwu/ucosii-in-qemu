#ifndef HW_VIRTIO_NET_H
#define HW_VIRTIO_NET_H

#include <stdint.h>

#define VIRTIO_NET_MAC_LEN 6

int virtio_net_init(const uint8_t mac[VIRTIO_NET_MAC_LEN]);
void virtio_net_poll(void (*handler)(const uint8_t *frame, uint16_t len));
int virtio_net_send(const uint8_t *frame, uint16_t len);

#endif /* HW_VIRTIO_NET_H */
