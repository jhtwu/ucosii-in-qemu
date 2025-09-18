#ifndef HW_TIMER_H
#define HW_TIMER_H

#include <stdint.h>

void timer_init(uint32_t tick_hz);
void timer_ack(void);

#endif /* HW_TIMER_H */
