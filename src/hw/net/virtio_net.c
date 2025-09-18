#include "hw/virtio_net.h"
#include "hw/virtio_mmio.h"
#include "hw/virtio_queue.h"
#include "hw/serial.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VIRTIO_NET_BASE            0x10001200u
#define VIRTIO_NET_DEVICE_ID       1u

#define VIRTQ_DESC_F_NEXT  0x1u
#define VIRTQ_DESC_F_WRITE 0x2u

#define VIRTIO_NET_RX_QUEUE 0u
#define VIRTIO_NET_TX_QUEUE 1u

#define VIRTIO_NET_QUEUE_SIZE      128u
#define VIRTIO_NET_RX_BUF_SIZE     2048u
#define VIRTIO_NET_TX_BUF_SIZE     2048u
#define VIRTIO_NET_HDR_SIZE        10u

#define VIRTIO_NET_F_MAC           (1u << 5)

struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

struct virtio_net_config {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
} __attribute__((packed));

static struct virtio_queue rx_queue;
static struct virtio_queue tx_queue;

static uint8_t rx_queue_mem[8192] __attribute__((aligned(4096)));
static uint8_t tx_queue_mem[8192] __attribute__((aligned(4096)));

static uint8_t rx_buffers[VIRTIO_NET_QUEUE_SIZE][VIRTIO_NET_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[VIRTIO_NET_QUEUE_SIZE][VIRTIO_NET_TX_BUF_SIZE] __attribute__((aligned(16)));

static uint16_t tx_free_stack[VIRTIO_NET_QUEUE_SIZE];
static uint16_t tx_free_count = 0;

static uint8_t virtio_net_mac[VIRTIO_NET_MAC_LEN];

static inline void memory_barrier(void) {
    __asm__ volatile ("dmb ish" ::: "memory");
}

static bool virtio_net_verify(void) {
    uint32_t magic = virtio_mmio_read32(VIRTIO_NET_BASE, VIRTIO_MMIO_MAGIC_VALUE);
    uint32_t version = virtio_mmio_read32(VIRTIO_NET_BASE, VIRTIO_MMIO_VERSION);
    uint32_t device = virtio_mmio_read32(VIRTIO_NET_BASE, VIRTIO_MMIO_DEVICE_ID);
    if (magic != 0x74726976u) {
        serial_write("[VIRTIO-NET] bad magic\n");
        return false;
    }
    if (version < 2u) {
        serial_write("[VIRTIO-NET] unsupported version\n");
        return false;
    }
    if (device != VIRTIO_NET_DEVICE_ID) {
        serial_write("[VIRTIO-NET] wrong device id\n");
        return false;
    }
    return true;
}

static bool virtio_net_setup_queue(uint16_t index, struct virtio_queue *queue, uint8_t *mem) {
    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_SEL, index);
    uint32_t max = virtio_mmio_read32(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0u) {
        serial_write("[VIRTIO-NET] queue unavailable\n");
        return false;
    }
    uint16_t qsz = VIRTIO_NET_QUEUE_SIZE;
    if (qsz > max) {
        qsz = (uint16_t)max;
    }
    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_NUM, qsz);

    virtio_queue_layout(mem, qsz, queue);
    virtio_queue_clear(queue);

    virtio_mmio_write_addr(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_DESC_LOW, VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint64_t)(uintptr_t)queue->desc);
    virtio_mmio_write_addr(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_AVAIL_LOW, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint64_t)(uintptr_t)queue->avail);
    virtio_mmio_write_addr(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_USED_LOW, VIRTIO_MMIO_QUEUE_USED_HIGH, (uint64_t)(uintptr_t)queue->used);

    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_READY, 1u);
    return true;
}

static void virtio_net_post_rx_buffers(void) {
    for (uint16_t i = 0; i < rx_queue.size; ++i) {
        rx_queue.desc[i].addr = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_queue.desc[i].len = VIRTIO_NET_RX_BUF_SIZE;
        rx_queue.desc[i].flags = VIRTQ_DESC_F_WRITE;
        rx_queue.desc[i].next = 0u;
        rx_queue.avail->ring[i] = i;
    }
    rx_queue.avail->idx = rx_queue.size;
    rx_queue.last_used_idx = 0u;
    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_NET_RX_QUEUE);
}

static void virtio_net_prepare_tx_queue(void) {
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
}

static void virtio_net_ack_interrupt(void) {
    uint32_t status = virtio_mmio_read32(VIRTIO_NET_BASE, VIRTIO_MMIO_INTERRUPT_STATUS);
    if (status != 0u) {
        virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_INTERRUPT_ACK, status);
    }
}

int virtio_net_init(const uint8_t mac[VIRTIO_NET_MAC_LEN]) {
    if (!virtio_net_verify()) {
        return -1;
    }

    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS, 0u);
    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS,
                        VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    uint32_t features = virtio_mmio_read32(VIRTIO_NET_BASE, VIRTIO_MMIO_DEVICE_FEATURES);
    uint32_t driver_features = 0u;
    if (features & VIRTIO_NET_F_MAC) {
        driver_features |= VIRTIO_NET_F_MAC;
    }
    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_DRIVER_FEATURES, driver_features);
    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS,
                        VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                        VIRTIO_STATUS_FEATURES_OK);

    uint32_t status = virtio_mmio_read32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS);
    if ((status & VIRTIO_STATUS_FEATURES_OK) == 0u) {
        serial_write("[VIRTIO-NET] feature negotiation failed\n");
        virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    if (!virtio_net_setup_queue(VIRTIO_NET_RX_QUEUE, &rx_queue, rx_queue_mem)) {
        virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }
    if (!virtio_net_setup_queue(VIRTIO_NET_TX_QUEUE, &tx_queue, tx_queue_mem)) {
        virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        return -1;
    }

    virtio_net_post_rx_buffers();
    virtio_net_prepare_tx_queue();

    if (driver_features & VIRTIO_NET_F_MAC) {
        const struct virtio_net_config *cfg =
            (const struct virtio_net_config *)((uintptr_t)VIRTIO_NET_BASE + VIRTIO_MMIO_CONFIG);
        for (int i = 0; i < VIRTIO_NET_MAC_LEN; ++i) {
            virtio_net_mac[i] = cfg->mac[i];
        }
    } else {
        for (int i = 0; i < VIRTIO_NET_MAC_LEN; ++i) {
            virtio_net_mac[i] = mac[i];
        }
    }

    bool mac_match = true;
    for (int i = 0; i < VIRTIO_NET_MAC_LEN; ++i) {
        if (virtio_net_mac[i] != mac[i]) {
            mac_match = false;
            break;
        }
    }
    if (!mac_match) {
        serial_write("[VIRTIO-NET] MAC differs from configuration\n");
    }

    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_STATUS,
                        VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
                        VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    serial_write("[VIRTIO-NET] initialized\n");
    return 0;
}

static void virtio_net_release_tx(void) {
    while (tx_queue.last_used_idx != tx_queue.used->idx) {
        uint16_t used = (uint16_t)(tx_queue.last_used_idx % tx_queue.size);
        uint32_t desc_id = tx_queue.used->ring[used].id;
        if (tx_free_count < tx_queue.size) {
            tx_free_stack[tx_free_count++] = (uint16_t)desc_id;
        }
        ++tx_queue.last_used_idx;
    }
}

void virtio_net_poll(void (*handler)(const uint8_t *frame, uint16_t len)) {
    virtio_net_ack_interrupt();
    virtio_net_release_tx();

    bool repost = false;
    while (rx_queue.last_used_idx != rx_queue.used->idx) {
        uint16_t used = (uint16_t)(rx_queue.last_used_idx % rx_queue.size);
        uint32_t desc_id = rx_queue.used->ring[used].id;
        uint32_t total_len = rx_queue.used->ring[used].len;
        if (total_len > VIRTIO_NET_HDR_SIZE) {
            uint16_t payload_len = (uint16_t)(total_len - VIRTIO_NET_HDR_SIZE);
            if (handler) {
                handler(rx_buffers[desc_id] + VIRTIO_NET_HDR_SIZE, payload_len);
            }
        }
        rx_queue.desc[desc_id].len = VIRTIO_NET_RX_BUF_SIZE;
        rx_queue.desc[desc_id].flags = VIRTQ_DESC_F_WRITE;
        uint16_t avail_index = (uint16_t)(rx_queue.avail->idx % rx_queue.size);
        rx_queue.avail->ring[avail_index] = (uint16_t)desc_id;
        rx_queue.avail->idx++;
        ++rx_queue.last_used_idx;
        repost = true;
    }
    if (repost) {
        memory_barrier();
        virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_NET_RX_QUEUE);
    }
}

int virtio_net_send(const uint8_t *frame, uint16_t len) {
    if (len == 0u || len > (VIRTIO_NET_TX_BUF_SIZE - VIRTIO_NET_HDR_SIZE)) {
        return -1;
    }

    virtio_net_release_tx();

    while (tx_free_count == 0u) {
        virtio_net_release_tx();
    }

    uint16_t desc_id = tx_free_stack[--tx_free_count];
    struct virtio_net_hdr *hdr = (struct virtio_net_hdr *)tx_buffers[desc_id];
    hdr->flags = 0u;
    hdr->gso_type = 0u;
    hdr->hdr_len = 0u;
    hdr->gso_size = 0u;
    hdr->csum_start = 0u;
    hdr->csum_offset = 0u;

    uint8_t *payload = tx_buffers[desc_id] + VIRTIO_NET_HDR_SIZE;
    for (uint16_t i = 0; i < len; ++i) {
        payload[i] = frame[i];
    }
    uint16_t frame_len = len;
    if (frame_len < 60u) {
        for (uint16_t i = frame_len; i < 60u; ++i) {
            payload[i] = 0u;
        }
        frame_len = 60u;
    }

    uint32_t total = (uint32_t)(frame_len + VIRTIO_NET_HDR_SIZE);
    tx_queue.desc[desc_id].len = total;
    tx_queue.desc[desc_id].flags = 0u;
    tx_queue.desc[desc_id].next = 0u;

    uint16_t avail_index = (uint16_t)(tx_queue.avail->idx % tx_queue.size);
    tx_queue.avail->ring[avail_index] = desc_id;
    memory_barrier();
    tx_queue.avail->idx++;
    memory_barrier();
    virtio_mmio_write32(VIRTIO_NET_BASE, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_NET_TX_QUEUE);
    return 0;
}
