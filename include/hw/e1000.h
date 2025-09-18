#ifndef HW_E1000_H
#define HW_E1000_H

#include <stdint.h>

#define E1000_MAC_LEN 6

int e1000_init(const uint8_t mac[E1000_MAC_LEN]);
void e1000_poll(void (*handler)(const uint8_t *frame, uint16_t len));
int e1000_send(const uint8_t *frame, uint16_t len);

#endif /* HW_E1000_H */
