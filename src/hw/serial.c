#include "hw/serial.h"
#include "hw/virtio_console.h"

#include <stddef.h>

void serial_init(void) {
    (void)virtio_console_init();
}

void serial_write_char(char c) {
    virtio_console_write(&c, 1u);
}

void serial_write(const char *s) {
    if (s == NULL) {
        return;
    }
    size_t len = 0u;
    while (s[len] != '\0') {
        ++len;
    }
    if (len > 0u) {
        virtio_console_write(s, len);
    }
}

void serial_write_hex(uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buffer[10];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 8; ++i) {
        uint32_t shift = (uint32_t)(28 - i * 4);
        buffer[2 + i] = hex[(value >> shift) & 0xFu];
    }
    virtio_console_write(buffer, sizeof(buffer));
}

void serial_write_dec(uint32_t value) {
    char buffer[12];
    int pos = 0;
    if (value == 0u) {
        buffer[pos++] = '0';
    } else {
        char tmp[10];
        int t = 0;
        while (value > 0u && t < 10) {
            tmp[t++] = (char)('0' + (value % 10u));
            value /= 10u;
        }
        while (t > 0) {
            buffer[pos++] = tmp[--t];
        }
    }
    virtio_console_write(buffer, (size_t)pos);
}
