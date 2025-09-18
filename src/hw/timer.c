#include "hw/timer.h"
#include <stdint.h>

static uint64_t timer_reload = 0;

void timer_init(uint32_t tick_hz) {
    uint64_t freq;
    __asm__ volatile ("mrs %0, cntfrq_el0" : "=r"(freq));
    if (tick_hz == 0u) {
        tick_hz = 1u;
    }
    timer_reload = freq / (uint64_t)tick_hz;
    if (timer_reload == 0u) {
        timer_reload = 1u;
    }
    __asm__ volatile ("msr cntv_tval_el0, %0" :: "r"(timer_reload));
    __asm__ volatile ("msr cntv_ctl_el0, %0" :: "r"((uint64_t)1));
}

void timer_ack(void) {
    __asm__ volatile ("msr cntv_tval_el0, %0" :: "r"(timer_reload));
}
