#ifndef HW_VIRTIO_MMIO_H
#define HW_VIRTIO_MMIO_H

#include <stdint.h>

#define VIRTIO_MMIO_MAGIC_VALUE          0x000
#define VIRTIO_MMIO_VERSION              0x004
#define VIRTIO_MMIO_DEVICE_ID            0x008
#define VIRTIO_MMIO_VENDOR_ID            0x00C
#define VIRTIO_MMIO_DEVICE_FEATURES      0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL  0x014
#define VIRTIO_MMIO_DRIVER_FEATURES      0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL  0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE      0x028 /* legacy */
#define VIRTIO_MMIO_QUEUE_SEL            0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX        0x034
#define VIRTIO_MMIO_QUEUE_NUM            0x038
#define VIRTIO_MMIO_QUEUE_READY          0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY         0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS     0x060
#define VIRTIO_MMIO_INTERRUPT_ACK        0x064
#define VIRTIO_MMIO_STATUS               0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW       0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH      0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW      0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH     0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW       0x0A0
#define VIRTIO_MMIO_QUEUE_USED_HIGH      0x0A4
#define VIRTIO_MMIO_CONFIG_GENERATION    0x0FC
#define VIRTIO_MMIO_CONFIG               0x100

#define VIRTIO_STATUS_ACKNOWLEDGE        0x01
#define VIRTIO_STATUS_DRIVER             0x02
#define VIRTIO_STATUS_DRIVER_OK          0x04
#define VIRTIO_STATUS_FEATURES_OK        0x08
#define VIRTIO_STATUS_FAILED             0x80

static inline void virtio_mmio_write32(uintptr_t base, uint32_t offset, uint32_t value) {
    *(volatile uint32_t *)(base + offset) = value;
}

static inline uint32_t virtio_mmio_read32(uintptr_t base, uint32_t offset) {
    return *(volatile uint32_t *)(base + offset);
}

static inline void virtio_mmio_write_addr(uintptr_t base, uint32_t low_offset, uint32_t high_offset, uint64_t value) {
    virtio_mmio_write32(base, low_offset, (uint32_t)(value & 0xFFFFFFFFull));
    virtio_mmio_write32(base, high_offset, (uint32_t)(value >> 32));
}

#endif /* HW_VIRTIO_MMIO_H */
