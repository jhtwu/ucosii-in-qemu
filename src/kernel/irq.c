#include "hw/gic.h"
#include "hw/timer.h"
#include "os_core.h"
#include "ucos_ii.h"

void irq_handler(void) {
    uint32_t ack = gic_acknowledge();

    if (ack == GIC_IRQ_VIRTUAL_TIMER) {
        timer_ack();
        OSIntEnter();
        OSTimeTick();
        OSIntExit();
    }

    gic_end_irq(ack);
}
