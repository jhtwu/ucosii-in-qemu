#ifndef HW_VIRTIO_CONSOLE_H
#define HW_VIRTIO_CONSOLE_H

#include <stddef.h>
#include <stdint.h>

int virtio_console_init(void);
void virtio_console_write(const char *buf, size_t len);
void virtio_console_irq(void);

#endif /* HW_VIRTIO_CONSOLE_H */
