#include "hw/gic.h"
#include <stdint.h>

#define GICD_BASE 0x08000000u
#define GICC_BASE 0x08010000u

#define GICD_CTLR           0x0000
#define GICD_ISENABLER(n)   (0x0100 + ((n) * 4))
#define GICD_ICENABLER(n)   (0x0180 + ((n) * 4))
#define GICD_IPRIORITYR(n)  (0x0400 + ((n) * 4))
#define GICD_ITARGETSR(n)   (0x0800 + ((n) * 4))
#define GICD_ICFGR(n)       (0x0C00 + ((n) * 4))

#define GICC_CTLR           0x0000
#define GICC_PMR            0x0004
#define GICC_BPR            0x0008
#define GICC_IAR            0x000C
#define GICC_EOIR           0x0010

static inline void mmio_write32(uint32_t addr, uint32_t value) {
    *(volatile uint32_t *)(uintptr_t)addr = value;
}

static inline uint32_t mmio_read32(uint32_t addr) {
    return *(volatile uint32_t *)(uintptr_t)addr;
}

void gic_init(void) {
    /* Disable distributor and CPU interface */
    mmio_write32(GICD_BASE + GICD_CTLR, 0u);
    mmio_write32(GICC_BASE + GICC_CTLR, 0u);

    /* Set priority mask to allow all priority levels */
    mmio_write32(GICC_BASE + GICC_PMR, 0xFFu);
    mmio_write32(GICC_BASE + GICC_BPR, 0u);

    /* Enable CPU interface and distributor */
    mmio_write32(GICC_BASE + GICC_CTLR, 1u);
    mmio_write32(GICD_BASE + GICD_CTLR, 1u);
}

static void gic_configure_irq(uint32_t irq) {
    uint32_t reg;
    uint32_t shift;

    /* Level-sensitive, active low (clear corresponding bits) */
    reg = irq / 16u;
    shift = (irq % 16u) * 2u;
    uint32_t icfgr = mmio_read32(GICD_BASE + GICD_ICFGR(reg));
    icfgr &= ~(0x3u << shift);
    mmio_write32(GICD_BASE + GICD_ICFGR(reg), icfgr);

    /* Route to CPU0 */
    reg = irq / 4u;
    shift = (irq % 4u) * 8u;
    uint32_t itarget = mmio_read32(GICD_BASE + GICD_ITARGETSR(reg));
    itarget &= ~(0xFFu << shift);
    itarget |= (1u << shift); /* CPU0 */
    mmio_write32(GICD_BASE + GICD_ITARGETSR(reg), itarget);

    /* Priority halfway */
    reg = irq / 4u;
    shift = (irq % 4u) * 8u;
    uint32_t iprio = mmio_read32(GICD_BASE + GICD_IPRIORITYR(reg));
    iprio &= ~(0xFFu << shift);
    iprio |= (0x80u << shift);
    mmio_write32(GICD_BASE + GICD_IPRIORITYR(reg), iprio);
}

void gic_enable_irq(uint32_t irq) {
    gic_configure_irq(irq);
    uint32_t reg = irq / 32u;
    uint32_t mask = 1u << (irq % 32u);
    mmio_write32(GICD_BASE + GICD_ICENABLER(reg), mask);
    mmio_write32(GICD_BASE + GICD_ISENABLER(reg), mask);
}

uint32_t gic_acknowledge(void) {
    return mmio_read32(GICC_BASE + GICC_IAR);
}

void gic_end_irq(uint32_t irq) {
    mmio_write32(GICC_BASE + GICC_EOIR, irq);
}
