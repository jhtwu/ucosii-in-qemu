#include "hw/e1000.h"
#include "hw/pci.h"
#include "hw/io.h"
#include "hw/serial.h"
#include "ucos_ii.h"
#include <stddef.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

#define E1000_REG_CTRL      0x00000
#define E1000_REG_STATUS    0x00008
#define E1000_REG_IMC       0x000D8
#define E1000_REG_RCTL      0x00100
#define E1000_REG_TCTL      0x00400
#define E1000_REG_TIPG      0x00410
#define E1000_REG_RAL       0x05400
#define E1000_REG_RAH       0x05404
#define E1000_REG_TDBAL     0x03800
#define E1000_REG_TDBAH     0x03804
#define E1000_REG_TDLEN     0x03808
#define E1000_REG_TDH       0x03810
#define E1000_REG_TDT       0x03818
#define E1000_REG_RDBAL     0x02800
#define E1000_REG_RDBAH     0x02804
#define E1000_REG_RDLEN     0x02808
#define E1000_REG_RDH       0x02810
#define E1000_REG_RDT       0x02818
#define E1000_REG_MTA       0x05200
#define E1000_REG_RSRPD     0x02C00

#define E1000_CTRL_RST      (1u << 26)
#define E1000_CTRL_SLU      (1u << 6)

#define E1000_RCTL_EN       (1u << 1)
#define E1000_RCTL_SBP      (1u << 2)
#define E1000_RCTL_UPE      (1u << 3)
#define E1000_RCTL_MPE      (1u << 4)
#define E1000_RCTL_LPE      (1u << 5)
#define E1000_RCTL_BAM      (1u << 15)
#define E1000_RCTL_BSIZE_2048 0
#define E1000_RCTL_SECRC    (1u << 26)

#define E1000_TCTL_EN       (1u << 1)
#define E1000_TCTL_PSP      (1u << 3)
#define E1000_TCTL_CT_SHIFT 4
#define E1000_TCTL_COLD_SHIFT 12

#define E1000_TX_CMD_EOP    (1u << 0)
#define E1000_TX_CMD_IFCS   (1u << 1)
#define E1000_TX_CMD_RS     (1u << 3)
#define E1000_TX_STATUS_DD  (1u << 0)

#define E1000_RX_STATUS_DD  (1u << 0)
#define E1000_RX_STATUS_EOP (1u << 1)

#define E1000_TX_RING_SIZE  8
#define E1000_RX_RING_SIZE  16
#define E1000_TX_BUF_SIZE   2048
#define E1000_RX_BUF_SIZE   2048

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t csum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

static volatile uint32_t *e1000_mmio = NULL;
static struct e1000_tx_desc tx_ring[E1000_TX_RING_SIZE] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_ring[E1000_RX_RING_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_TX_RING_SIZE][E1000_TX_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t rx_buffers[E1000_RX_RING_SIZE][E1000_RX_BUF_SIZE] __attribute__((aligned(16)));
static uint32_t tx_tail = 0;
static uint32_t rx_tail = 0;
static uint8_t e1000_mac[E1000_MAC_LEN];

static inline void e1000_write(uint32_t reg, uint32_t value) {
    e1000_mmio[reg / 4] = value;
}

static inline uint32_t e1000_read(uint32_t reg) {
    return e1000_mmio[reg / 4];
}

static void e1000_reset(void) {
    e1000_write(E1000_REG_IMC, 0xFFFFFFFFu);
    e1000_write(E1000_REG_RCTL, 0);
    e1000_write(E1000_REG_TCTL, 0);
    e1000_write(E1000_REG_CTRL, E1000_CTRL_RST);
    while (e1000_read(E1000_REG_CTRL) & E1000_CTRL_RST) {
    }
}

static void e1000_mac_write(const uint8_t mac[E1000_MAC_LEN]) {
    uint32_t ral = (uint32_t)mac[0] |
                   ((uint32_t)mac[1] << 8) |
                   ((uint32_t)mac[2] << 16) |
                   ((uint32_t)mac[3] << 24);
    uint32_t rah = (uint32_t)mac[4] |
                   ((uint32_t)mac[5] << 8) |
                   (1u << 31);
    e1000_write(E1000_REG_RAL, ral);
    e1000_write(E1000_REG_RAH, rah);
}

static void e1000_clear_mta(void) {
    for (uint32_t i = 0; i < 128; ++i) {
        e1000_write(E1000_REG_MTA + i * 4, 0);
    }
}

int e1000_init(const uint8_t mac[E1000_MAC_LEN]) {
    struct pci_device dev;
    if (pci_find_device(E1000_VENDOR_ID, E1000_DEVICE_ID, &dev) != 0) {
        serial_write("[E1000] device not found\n");
        return -1;
    }

    pci_enable_bus_master(&dev);

    uint32_t bar0 = dev.bar[0];
    if (!(bar0 & 0x1u)) {
        uintptr_t base = (uintptr_t)(bar0 & 0xFFFFFFF0u);
        e1000_mmio = (volatile uint32_t *)base;
    } else {
        serial_write("[E1000] unexpected I/O BAR\n");
        return -1;
    }

    e1000_reset();

    for (int i = 0; i < E1000_MAC_LEN; ++i) {
        e1000_mac[i] = mac[i];
    }
    e1000_mac_write(e1000_mac);
    e1000_clear_mta();

    for (int i = 0; i < E1000_TX_RING_SIZE; ++i) {
        tx_ring[i].addr = (uint64_t)(uintptr_t)tx_buffers[i];
        tx_ring[i].length = 0;
        tx_ring[i].cso = 0;
        tx_ring[i].cmd = 0;
        tx_ring[i].status = E1000_TX_STATUS_DD;
        tx_ring[i].css = 0;
        tx_ring[i].special = 0;
    }
    tx_tail = 0;

    for (int i = 0; i < E1000_RX_RING_SIZE; ++i) {
        rx_ring[i].addr = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_ring[i].status = 0;
    }
    rx_tail = 0;

    e1000_write(E1000_REG_TDBAL, (uint32_t)(uintptr_t)tx_ring);
    e1000_write(E1000_REG_TDBAH, 0);
    e1000_write(E1000_REG_TDLEN, E1000_TX_RING_SIZE * sizeof(struct e1000_tx_desc));
    e1000_write(E1000_REG_TDH, 0);
    e1000_write(E1000_REG_TDT, 0);

    e1000_write(E1000_REG_RDBAL, (uint32_t)(uintptr_t)rx_ring);
    e1000_write(E1000_REG_RDBAH, 0);
    e1000_write(E1000_REG_RDLEN, E1000_RX_RING_SIZE * sizeof(struct e1000_rx_desc));
    e1000_write(E1000_REG_RDH, 0);
    e1000_write(E1000_REG_RDT, E1000_RX_RING_SIZE - 1);

    e1000_write(E1000_REG_TCTL,
                E1000_TCTL_EN |
                E1000_TCTL_PSP |
                (0x10u << E1000_TCTL_CT_SHIFT) |
                (0x40u << E1000_TCTL_COLD_SHIFT));
    e1000_write(E1000_REG_TIPG, 0x0060200Au);

    e1000_write(E1000_REG_RCTL,
                E1000_RCTL_EN |
                E1000_RCTL_BAM |
                E1000_RCTL_SECRC |
                E1000_RCTL_BSIZE_2048);

    serial_write("[E1000] initialized\n");
    return 0;
}

void e1000_poll(void (*handler)(const uint8_t *frame, uint16_t len)) {
    while (1) {
        struct e1000_rx_desc *desc = &rx_ring[rx_tail];
        if (!(desc->status & E1000_RX_STATUS_DD)) {
            break;
        }
        if (!(desc->status & E1000_RX_STATUS_EOP)) {
            serial_write("[E1000] RX fragment\n");
        } else {
            uint16_t length = desc->length;
            if (handler) {
                handler(rx_buffers[rx_tail], length);
            }
        }
        desc->status = 0;
        rx_tail = (rx_tail + 1) % E1000_RX_RING_SIZE;
        e1000_write(E1000_REG_RDT, (rx_tail == 0) ? (E1000_RX_RING_SIZE - 1) : (rx_tail - 1));
    }
}

int e1000_send(const uint8_t *frame, uint16_t len) {
    if (len == 0 || len > E1000_TX_BUF_SIZE) {
        return -1;
    }
    uint32_t index = tx_tail;
    struct e1000_tx_desc *desc = &tx_ring[index];

    if (!(desc->status & E1000_TX_STATUS_DD)) {
        serial_write("[E1000] TX ring full\n");
        return -1;
    }

    for (uint16_t i = 0; i < len; ++i) {
        tx_buffers[index][i] = frame[i];
    }

    if (len < 60) {
        for (uint16_t i = len; i < 60; ++i) {
            tx_buffers[index][i] = 0;
        }
        len = 60;
    }

    desc->length = len;
    desc->cmd = E1000_TX_CMD_EOP | E1000_TX_CMD_IFCS | E1000_TX_CMD_RS;
    desc->status = 0;

    tx_tail = (tx_tail + 1) % E1000_TX_RING_SIZE;
    e1000_write(E1000_REG_TDT, tx_tail);

    while (!(desc->status & E1000_TX_STATUS_DD)) {
    }
    return 0;
}
