#ifndef HW_CPU_H
#define HW_CPU_H

#include <stdint.h>

static inline void cpu_enable_irq(void) {
    __asm__ volatile ("msr daifclr, #0x2" ::: "memory");
}

static inline void cpu_disable_irq(void) {
    __asm__ volatile ("msr daifset, #0x2" ::: "memory");
}

static inline void cpu_wait_for_interrupt(void) {
    __asm__ volatile ("wfi");
}

#endif /* HW_CPU_H */
