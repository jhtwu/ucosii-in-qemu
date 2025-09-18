#ifndef HW_GIC_H
#define HW_GIC_H

#include <stdint.h>

#define GIC_IRQ_VIRTUAL_TIMER   27u
#define GIC_IRQ_VIRTIO_CONSOLE  32u
#define GIC_IRQ_VIRTIO_NET      33u

void gic_init(void);
void gic_enable_irq(uint32_t irq);
uint32_t gic_acknowledge(void);
void gic_end_irq(uint32_t irq);

#endif /* HW_GIC_H */
