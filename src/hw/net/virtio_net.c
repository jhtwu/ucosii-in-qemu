#include "hw/virtio_net.h"
#include "hw/pci.h"
#include "hw/io.h"
#include "hw/serial.h"

#include <stddef.h>
#include <stdint.h>

#define VIRTIO_PCI_VENDOR_ID          0x1AF4
#define VIRTIO_PCI_NET_DEVICE_ID      0x1000

#define VIRTIO_PCI_HOST_FEATURES      0x00
#define VIRTIO_PCI_GUEST_FEATURES     0x04
#define VIRTIO_PCI_QUEUE_PFN          0x08
#define VIRTIO_PCI_QUEUE_NUM          0x0C
#define VIRTIO_PCI_QUEUE_SEL          0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY       0x10
#define VIRTIO_PCI_STATUS             0x12
#define VIRTIO_PCI_ISR                0x13
#define VIRTIO_PCI_CONFIG             0x14

#define VIRTIO_STATUS_ACKNOWLEDGE     0x01
#define VIRTIO_STATUS_DRIVER          0x02
#define VIRTIO_STATUS_DRIVER_OK       0x04
#define VIRTIO_STATUS_FEATURES_OK     0x08
#define VIRTIO_STATUS_FAILED          0x80

#define VIRTIO_NET_F_MAC              (1u << 5)

#define VIRTQ_DESC_F_NEXT             0x0001
#define VIRTQ_DESC_F_WRITE            0x0002

#define VIRTQ_ALIGN                   4096u
#define VIRTQ_MAX                     256u
#define VIRTQ_MEM_SIZE                16384u

#define VIRTIO_NET_RX_BUF_SIZE        2048u
#define VIRTIO_NET_TX_BUF_SIZE        2048u
#define VIRTIO_NET_HDR_SIZE           10u

#define VIRTQ_DESC_INVALID            0xFFFFu

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed));

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
} __attribute__((packed));

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[];
} __attribute__((packed));

struct virtio_queue {
    uint16_t size;
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
    uint16_t last_used_idx;
};

struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

static uint16_t virtio_io_base = 0;
static struct virtio_queue rx_queue;
static struct virtio_queue tx_queue;

static uint8_t rx_queue_mem[VIRTQ_MEM_SIZE] __attribute__((aligned(VIRTQ_ALIGN)));
static uint8_t tx_queue_mem[VIRTQ_MEM_SIZE] __attribute__((aligned(VIRTQ_ALIGN)));

static uint8_t rx_buffers[VIRTQ_MAX][VIRTIO_NET_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[VIRTQ_MAX][VIRTIO_NET_TX_BUF_SIZE] __attribute__((aligned(16)));

static uint16_t tx_free_head = VIRTQ_DESC_INVALID;
static uint16_t tx_free_count = 0;
static uint8_t tx_in_use[VIRTQ_MAX];

static uint8_t virtio_mac[VIRTIO_NET_MAC_LEN];

static void mem_clear(uint8_t *ptr, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ptr[i] = 0;
    }
}

static void mem_copy(uint8_t *dst, const uint8_t *src, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];
    }
}

static size_t virtq_avail_size(uint16_t qsz) {
    return (size_t)(sizeof(struct virtq_avail) + sizeof(uint16_t) * qsz + sizeof(uint16_t));
}

static size_t virtq_used_size(uint16_t qsz) {
    return (size_t)(sizeof(struct virtq_used) + sizeof(struct virtq_used_elem) * qsz + sizeof(uint16_t));
}

static void virtio_set_status(uint8_t bit) {
    uint8_t status = inb(virtio_io_base + VIRTIO_PCI_STATUS);
    status |= bit;
    outb(virtio_io_base + VIRTIO_PCI_STATUS, status);
}

static void virtio_reset_status(void) {
    outb(virtio_io_base + VIRTIO_PCI_STATUS, 0);
}

static void virtio_queue_layout(uint8_t *mem, uint16_t qsz, struct virtio_queue *queue) {
    uintptr_t base = (uintptr_t)mem;
    uintptr_t desc_addr = base;
    uintptr_t avail_addr = desc_addr + (uintptr_t)(sizeof(struct virtq_desc) * qsz);
    uintptr_t used_addr = (avail_addr + virtq_avail_size(qsz) + (VIRTQ_ALIGN - 1)) & ~(uintptr_t)(VIRTQ_ALIGN - 1);

    queue->size = qsz;
    queue->desc = (struct virtq_desc *)desc_addr;
    queue->avail = (struct virtq_avail *)avail_addr;
    queue->used = (struct virtq_used *)used_addr;
    queue->last_used_idx = 0;
}

static void virtio_queue_clear(struct virtio_queue *queue) {
    size_t desc_bytes = sizeof(struct virtq_desc) * queue->size;
    size_t avail_bytes = virtq_avail_size(queue->size);
    size_t used_bytes = virtq_used_size(queue->size);

    mem_clear((uint8_t *)queue->desc, desc_bytes);
    mem_clear((uint8_t *)queue->avail, avail_bytes);
    mem_clear((uint8_t *)queue->used, used_bytes);
}

static int virtio_queue_configure(uint16_t index, uint8_t *mem, struct virtio_queue *queue) {
    outw(virtio_io_base + VIRTIO_PCI_QUEUE_SEL, index);
    uint16_t qsz = inw(virtio_io_base + VIRTIO_PCI_QUEUE_NUM);
    if (qsz == 0 || qsz > VIRTQ_MAX) {
        serial_write("[VIRTIO] invalid queue size\n");
        return -1;
    }

    virtio_queue_layout(mem, qsz, queue);
    virtio_queue_clear(queue);

    uint32_t phys = (uint32_t)((uintptr_t)mem >> 12);
    outl(virtio_io_base + VIRTIO_PCI_QUEUE_PFN, phys);
    return 0;
}

static void virtio_queue_notify(uint16_t index) {
    outw(virtio_io_base + VIRTIO_PCI_QUEUE_NOTIFY, index);
}

static void virtio_service_tx(void) {
    if (tx_queue.size == 0) {
        return;
    }
    while (tx_queue.last_used_idx != tx_queue.used->idx) {
        uint16_t used_idx = (uint16_t)(tx_queue.last_used_idx % tx_queue.size);
        struct virtq_used_elem *elem = &tx_queue.used->ring[used_idx];
        uint16_t desc_id = (uint16_t)(elem->id & 0xFFFFu);

        tx_in_use[desc_id] = 0;
        tx_queue.desc[desc_id].next = tx_free_head;
        tx_free_head = desc_id;
        if (tx_free_count < tx_queue.size) {
            ++tx_free_count;
        }

        tx_queue.last_used_idx++;
    }
}

static void virtio_post_all_rx_buffers(void) {
    if (rx_queue.size == 0) {
        return;
    }
    for (uint16_t i = 0; i < rx_queue.size; ++i) {
        rx_queue.desc[i].addr = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_queue.desc[i].len = VIRTIO_NET_RX_BUF_SIZE;
        rx_queue.desc[i].flags = VIRTQ_DESC_F_WRITE;
        rx_queue.desc[i].next = 0;
        rx_queue.avail->ring[i] = i;
    }
    rx_queue.avail->idx = rx_queue.size;
    rx_queue.avail->flags = 0;
    rx_queue.last_used_idx = 0;
    rx_queue.used->idx = 0;
    virtio_queue_notify(0);
}

static void virtio_prepare_tx_queue(void) {
    if (tx_queue.size == 0) {
        return;
    }
    for (uint16_t i = 0; i < tx_queue.size; ++i) {
        tx_queue.desc[i].addr = (uint64_t)(uintptr_t)tx_buffers[i];
        tx_queue.desc[i].len = 0;
        tx_queue.desc[i].flags = 0;
        tx_queue.desc[i].next = (i + 1u < tx_queue.size) ? (uint16_t)(i + 1u) : VIRTQ_DESC_INVALID;
        tx_in_use[i] = 0;
    }
    tx_queue.avail->idx = 0;
    tx_queue.avail->flags = 0;
    tx_queue.used->idx = 0;
    tx_queue.last_used_idx = 0;
    tx_free_head = 0;
    tx_free_count = tx_queue.size;
}

int virtio_net_init(const uint8_t mac[VIRTIO_NET_MAC_LEN]) {
    struct pci_device dev;
    if (pci_find_device(VIRTIO_PCI_VENDOR_ID, VIRTIO_PCI_NET_DEVICE_ID, &dev) != 0) {
        serial_write("[VIRTIO] device not found\n");
        return -1;
    }

    pci_enable_bus_master(&dev);

    uint32_t bar0 = dev.bar[0];
    if ((bar0 & 0x1u) == 0) {
        serial_write("[VIRTIO] BAR0 is not IO space\n");
        return -1;
    }

    virtio_io_base = (uint16_t)(bar0 & 0xFFFCu);

    virtio_reset_status();
    virtio_set_status(VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_set_status(VIRTIO_STATUS_DRIVER);

    uint32_t host_features = inl(virtio_io_base + VIRTIO_PCI_HOST_FEATURES);
    uint32_t guest_features = 0u;
    if (host_features & VIRTIO_NET_F_MAC) {
        guest_features |= VIRTIO_NET_F_MAC;
    }
    outl(virtio_io_base + VIRTIO_PCI_GUEST_FEATURES, guest_features);
    virtio_set_status(VIRTIO_STATUS_FEATURES_OK);

    uint8_t status = inb(virtio_io_base + VIRTIO_PCI_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        serial_write("[VIRTIO] feature negotiation failed\n");
        return -1;
    }

    if (virtio_queue_configure(0, rx_queue_mem, &rx_queue) != 0) {
        return -1;
    }
    if (virtio_queue_configure(1, tx_queue_mem, &tx_queue) != 0) {
        return -1;
    }

    virtio_post_all_rx_buffers();
    virtio_prepare_tx_queue();

    if (guest_features & VIRTIO_NET_F_MAC) {
        for (int i = 0; i < VIRTIO_NET_MAC_LEN; ++i) {
            virtio_mac[i] = inb(virtio_io_base + VIRTIO_PCI_CONFIG + i);
        }
    } else {
        for (int i = 0; i < VIRTIO_NET_MAC_LEN; ++i) {
            virtio_mac[i] = mac[i];
        }
    }

    int mac_match = 1;
    for (int i = 0; i < VIRTIO_NET_MAC_LEN; ++i) {
        if (virtio_mac[i] != mac[i]) {
            mac_match = 0;
        }
    }
    if (!mac_match) {
        serial_write("[VIRTIO] MAC differs from configuration\n");
    }

    virtio_set_status(VIRTIO_STATUS_DRIVER_OK);
    serial_write("[VIRTIO] initialized\n");
    return 0;
}

void virtio_net_poll(void (*handler)(const uint8_t *frame, uint16_t len)) {
    virtio_service_tx();

    if (rx_queue.size == 0) {
        return;
    }

    while (rx_queue.last_used_idx != rx_queue.used->idx) {
        uint16_t used_idx = (uint16_t)(rx_queue.last_used_idx % rx_queue.size);
        struct virtq_used_elem *elem = &rx_queue.used->ring[used_idx];
        uint16_t desc_id = (uint16_t)(elem->id & 0xFFFFu);
        uint32_t total_len = elem->len;

        if (total_len > VIRTIO_NET_HDR_SIZE) {
            uint16_t payload_len = (uint16_t)(total_len - VIRTIO_NET_HDR_SIZE);
            if (handler) {
                handler(rx_buffers[desc_id] + VIRTIO_NET_HDR_SIZE, payload_len);
            }
        }

        rx_queue.desc[desc_id].len = VIRTIO_NET_RX_BUF_SIZE;
        rx_queue.desc[desc_id].flags = VIRTQ_DESC_F_WRITE;
        rx_queue.desc[desc_id].next = 0;

        uint16_t avail_idx = (uint16_t)(rx_queue.avail->idx % rx_queue.size);
        rx_queue.avail->ring[avail_idx] = desc_id;
        rx_queue.avail->idx++;

        rx_queue.last_used_idx++;
        virtio_queue_notify(0);
    }
}

int virtio_net_send(const uint8_t *frame, uint16_t len) {
    if (tx_queue.size == 0) {
        return -1;
    }

    virtio_service_tx();

    if (tx_free_count == 0) {
        virtio_service_tx();
        if (tx_free_count == 0) {
            return -1;
        }
    }

    if (len == 0) {
        return -1;
    }

    uint16_t desc_id = tx_free_head;
    if (desc_id == VIRTQ_DESC_INVALID) {
        return -1;
    }

    tx_free_head = tx_queue.desc[desc_id].next;
    if (tx_free_count > 0) {
        --tx_free_count;
    }
    tx_in_use[desc_id] = 1;

    uint16_t frame_len = len;
    if (frame_len > (VIRTIO_NET_TX_BUF_SIZE - VIRTIO_NET_HDR_SIZE)) {
        tx_in_use[desc_id] = 0;
        tx_queue.desc[desc_id].next = tx_free_head;
        tx_free_head = desc_id;
        if (tx_free_count < tx_queue.size) {
            ++tx_free_count;
        }
        return -1;
    }

    struct virtio_net_hdr *hdr = (struct virtio_net_hdr *)tx_buffers[desc_id];
    hdr->flags = 0;
    hdr->gso_type = 0;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;

    uint8_t *payload = tx_buffers[desc_id] + VIRTIO_NET_HDR_SIZE;
    mem_copy(payload, frame, frame_len);

    if (frame_len < 60u) {
        for (uint16_t i = frame_len; i < 60u; ++i) {
            payload[i] = 0;
        }
        frame_len = 60u;
    }

    uint32_t total_len = (uint32_t)(frame_len + VIRTIO_NET_HDR_SIZE);
    tx_queue.desc[desc_id].addr = (uint64_t)(uintptr_t)tx_buffers[desc_id];
    tx_queue.desc[desc_id].len = total_len;
    tx_queue.desc[desc_id].flags = 0;
    tx_queue.desc[desc_id].next = 0;

    uint16_t avail_idx = (uint16_t)(tx_queue.avail->idx % tx_queue.size);
    tx_queue.avail->ring[avail_idx] = desc_id;
    tx_queue.avail->idx++;

    virtio_queue_notify(1);
    return 0;
}
