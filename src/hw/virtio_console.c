#include "hw/virtio_console.h"
#include "hw/virtio_mmio.h"
#include "hw/virtio_queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VIRTIO_CONSOLE_BASE      0x10001000u
#define VIRTIO_CONSOLE_DEVICE_ID 3u
#define VIRTIO_CONSOLE_VENDOR_ID 0x554D4551u /* QEMU */

#define VIRTQ_DESC_F_NEXT  0x1u
#define VIRTQ_DESC_F_WRITE 0x2u

#define VIRTIO_CONSOLE_QUEUE_SIZE 16u
#define VIRTIO_CONSOLE_RX_BUF_SIZE 256u
#define VIRTIO_CONSOLE_TX_BUF_SIZE 256u

static struct virtio_queue rx_queue;
static struct virtio_queue tx_queue;

static uint8_t rx_queue_mem[4096] __attribute__((aligned(4096)));
static uint8_t tx_queue_mem[4096] __attribute__((aligned(4096)));

static uint8_t rx_buffers[VIRTIO_CONSOLE_QUEUE_SIZE][VIRTIO_CONSOLE_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[VIRTIO_CONSOLE_QUEUE_SIZE][VIRTIO_CONSOLE_TX_BUF_SIZE] __attribute__((aligned(16)));

static uint16_t tx_free_stack[VIRTIO_CONSOLE_QUEUE_SIZE];
static uint16_t tx_free_count = 0;

static inline void memory_barrier(void) {
    __asm__ volatile ("dmb ish" ::: "memory");
}

static int virtio_console_verify(void) {
    uint32_t magic = virtio_mmio_read32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_MAGIC_VALUE);
    uint32_t version = virtio_mmio_read32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_VERSION);
    uint32_t device = virtio_mmio_read32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_DEVICE_ID);
    uint32_t vendor = virtio_mmio_read32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_VENDOR_ID);
    if (magic != 0x74726976u) {
        return -1;
    }
    if (version < 2u) {
        return -1;
    }
    if (device != VIRTIO_CONSOLE_DEVICE_ID) {
        return -1;
    }
    (void)vendor;
    return 0;
}

static int virtio_console_setup_queue(uint16_t index, struct virtio_queue *queue, uint8_t *mem, uint16_t requested_size) {
    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_SEL, index);
    uint32_t max = virtio_mmio_read32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0u) {
        return -1;
    }
    uint16_t qsz = requested_size;
    if (qsz > max) {
        qsz = (uint16_t)max;
    }
    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_NUM, qsz);

    virtio_queue_layout(mem, qsz, queue);
    virtio_queue_clear(queue);

    virtio_mmio_write_addr(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_DESC_LOW, VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint64_t)(uintptr_t)queue->desc);
    virtio_mmio_write_addr(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_AVAIL_LOW, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint64_t)(uintptr_t)queue->avail);
    virtio_mmio_write_addr(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_USED_LOW, VIRTIO_MMIO_QUEUE_USED_HIGH, (uint64_t)(uintptr_t)queue->used);

    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_READY, 1u);
    return 0;
}

int virtio_console_init(void) {
    if (virtio_console_verify() != 0) {
        return -1;
    }

    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS, 0u);
    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0u);
    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_DRIVER_FEATURES, 0u);
    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS,
                         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    uint32_t status = virtio_mmio_read32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS);
    if ((status & VIRTIO_STATUS_FEATURES_OK) == 0u) {
        virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    if (virtio_console_setup_queue(0u, &rx_queue, rx_queue_mem, VIRTIO_CONSOLE_QUEUE_SIZE) != 0) {
        virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }
    if (virtio_console_setup_queue(1u, &tx_queue, tx_queue_mem, VIRTIO_CONSOLE_QUEUE_SIZE) != 0) {
        virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    for (uint16_t i = 0; i < rx_queue.size; ++i) {
        rx_queue.desc[i].addr = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_queue.desc[i].len = VIRTIO_CONSOLE_RX_BUF_SIZE;
        rx_queue.desc[i].flags = VIRTQ_DESC_F_WRITE;
        rx_queue.desc[i].next = 0u;
        rx_queue.avail->ring[i] = i;
    }
    rx_queue.avail->idx = rx_queue.size;
    rx_queue.last_used_idx = 0u;
    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_NOTIFY, 0u);

    for (uint16_t i = 0; i < tx_queue.size; ++i) {
        tx_queue.desc[i].addr = (uint64_t)(uintptr_t)tx_buffers[i];
        tx_queue.desc[i].len = 0u;
        tx_queue.desc[i].flags = 0u;
        tx_queue.desc[i].next = 0u;
        tx_free_stack[i] = (uint16_t)(tx_queue.size - 1u - i);
    }
    tx_free_count = tx_queue.size;
    tx_queue.avail->idx = 0u;
    tx_queue.last_used_idx = 0u;

    virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_STATUS,
                         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                         VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    return 0;
}

void virtio_console_irq(void) {
    uint32_t pending = virtio_mmio_read32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_INTERRUPT_STATUS);
    if (pending != 0u) {
        virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_INTERRUPT_ACK, pending);
    }

    while (tx_queue.last_used_idx != tx_queue.used->idx) {
        uint16_t used = (uint16_t)(tx_queue.last_used_idx % tx_queue.size);
        uint32_t desc_id = tx_queue.used->ring[used].id;
        if (tx_free_count < tx_queue.size) {
            tx_free_stack[tx_free_count++] = (uint16_t)desc_id;
        }
        ++tx_queue.last_used_idx;
    }

    bool repost = false;
    while (rx_queue.last_used_idx != rx_queue.used->idx) {
        uint16_t used = (uint16_t)(rx_queue.last_used_idx % rx_queue.size);
        uint32_t desc_id = rx_queue.used->ring[used].id;
        rx_queue.desc[desc_id].len = VIRTIO_CONSOLE_RX_BUF_SIZE;
        rx_queue.desc[desc_id].flags = VIRTQ_DESC_F_WRITE;
        uint16_t avail_index = (uint16_t)(rx_queue.avail->idx % rx_queue.size);
        rx_queue.avail->ring[avail_index] = (uint16_t)desc_id;
        rx_queue.avail->idx++;
        ++rx_queue.last_used_idx;
        repost = true;
    }
    if (repost) {
        virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_NOTIFY, 0u);
    }
}

void virtio_console_write(const char *buf, size_t len) {
    if (buf == NULL || len == 0u) {
        return;
    }

    size_t offset = 0u;
    while (offset < len) {
        virtio_console_irq();
        if (tx_free_count == 0u) {
            continue;
        }
        size_t chunk = len - offset;
        if (chunk > VIRTIO_CONSOLE_TX_BUF_SIZE) {
            chunk = VIRTIO_CONSOLE_TX_BUF_SIZE;
        }
        uint16_t desc_id = tx_free_stack[--tx_free_count];
        uint8_t *dst = tx_buffers[desc_id];
        for (size_t i = 0; i < chunk; ++i) {
            dst[i] = (uint8_t)buf[offset + i];
        }
        tx_queue.desc[desc_id].len = (uint32_t)chunk;
        tx_queue.desc[desc_id].flags = 0u;
        tx_queue.desc[desc_id].next = 0u;

        uint16_t avail_index = (uint16_t)(tx_queue.avail->idx % tx_queue.size);
        tx_queue.avail->ring[avail_index] = desc_id;
        memory_barrier();
        tx_queue.avail->idx++;
        memory_barrier();
        virtio_mmio_write32(VIRTIO_CONSOLE_BASE, VIRTIO_MMIO_QUEUE_NOTIFY, 1u);
        offset += chunk;
    }
}
