#ifndef HW_VIRTIO_QUEUE_H
#define HW_VIRTIO_QUEUE_H

#include <stdint.h>
#include <stddef.h>

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

size_t virtio_queue_mem_size(uint16_t qsz);
void virtio_queue_layout(void *mem, uint16_t qsz, struct virtio_queue *queue);
void virtio_queue_clear(struct virtio_queue *queue);

#endif /* HW_VIRTIO_QUEUE_H */
