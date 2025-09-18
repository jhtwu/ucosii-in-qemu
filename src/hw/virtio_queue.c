#include "hw/virtio_queue.h"

static size_t align_up(size_t value, size_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

size_t virtio_queue_mem_size(uint16_t qsz) {
    size_t desc_bytes = sizeof(struct virtq_desc) * qsz;
    size_t avail_bytes = sizeof(struct virtq_avail) + sizeof(uint16_t) * qsz + sizeof(uint16_t);
    size_t used_bytes = sizeof(struct virtq_used) + sizeof(struct virtq_used_elem) * qsz;

    size_t offset = align_up(desc_bytes, 16u);
    offset = align_up(offset + avail_bytes, 2u);
    offset = align_up(offset, 4096u);
    offset += align_up(used_bytes, 4u);
    return offset;
}

void virtio_queue_layout(void *mem, uint16_t qsz, struct virtio_queue *queue) {
    uintptr_t base = (uintptr_t)mem;
    size_t desc_bytes = sizeof(struct virtq_desc) * qsz;
    size_t avail_bytes = sizeof(struct virtq_avail) + sizeof(uint16_t) * qsz + sizeof(uint16_t);

    uintptr_t desc_addr = base;
    uintptr_t avail_addr = align_up(desc_addr + desc_bytes, 2u);
    uintptr_t used_addr = align_up(avail_addr + avail_bytes, 4096u);

    queue->size = qsz;
    queue->desc = (struct virtq_desc *)desc_addr;
    queue->avail = (struct virtq_avail *)avail_addr;
    queue->used = (struct virtq_used *)used_addr;
    queue->last_used_idx = 0;
}

void virtio_queue_clear(struct virtio_queue *queue) {
    size_t desc_bytes = sizeof(struct virtq_desc) * queue->size;
    size_t avail_bytes = sizeof(struct virtq_avail) + sizeof(uint16_t) * queue->size + sizeof(uint16_t);
    size_t used_bytes = sizeof(struct virtq_used) + sizeof(struct virtq_used_elem) * queue->size;

    uint8_t *desc = (uint8_t *)queue->desc;
    for (size_t i = 0; i < desc_bytes; ++i) {
        desc[i] = 0;
    }
    uint8_t *avail = (uint8_t *)queue->avail;
    for (size_t i = 0; i < avail_bytes; ++i) {
        avail[i] = 0;
    }
    uint8_t *used = (uint8_t *)queue->used;
    for (size_t i = 0; i < used_bytes; ++i) {
        used[i] = 0;
    }
}
