#include "hw/serial.h"
#include "hw/io.h"

#define COM1_PORT 0x3F8

static int serial_initialized = 0;

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);    /* Disable all interrupts */
    outb(COM1_PORT + 3, 0x80);    /* Enable DLAB */
    outb(COM1_PORT + 0, 0x03);    /* Set baud rate divisor to 3 (38400 baud) */
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 2, 0xC7);    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
    serial_initialized = 1;
}

static int serial_is_transmit_empty(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_write_char(char c) {
    if (!serial_initialized) {
        serial_init();
    }
    while (!serial_is_transmit_empty()) {
    }
    outb(COM1_PORT, (uint8_t)c);
}

void serial_write(const char *s) {
    while (*s) {
        if (*s == '\n') {
            serial_write_char('\r');
        }
        serial_write_char(*s++);
    }
}

void serial_write_hex(uint32_t value) {
    const char digits[] = "0123456789ABCDEF";
    serial_write("0x");
    for (int i = 28; i >= 0; i -= 4) {
        serial_write_char(digits[(value >> i) & 0xF]);
    }
}

void serial_write_dec(uint32_t value) {
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    if (value == 0) {
        buf[--i] = '0';
    }
    while (value > 0 && i > 0) {
        buf[--i] = '0' + (value % 10);
        value /= 10;
    }
    serial_write(&buf[i]);
}
