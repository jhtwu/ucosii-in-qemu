#include "hw/pci.h"
#include "hw/io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static uint32_t pci_config_address(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    return (uint32_t)(0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                      ((uint32_t)function << 8) | (offset & 0xFCu));
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, slot, function, offset));
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, slot, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

int pci_find_device(uint16_t vendor, uint16_t device, struct pci_device *out) {
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint16_t slot = 0; slot < 32; ++slot) {
            for (uint16_t func = 0; func < 8; ++func) {
                uint32_t vendor_device = pci_config_read32(bus, slot, func, 0x00);
                uint16_t ven = (uint16_t)(vendor_device & 0xFFFFu);
                if (ven == 0xFFFFu) {
                    if (func == 0) {
                        break; /* no device present */
                    }
                    continue;
                }
                uint16_t dev = (uint16_t)((vendor_device >> 16) & 0xFFFFu);
                if (ven == vendor && dev == device) {
                    if (out) {
                        out->bus = bus;
                        out->slot = slot;
                        out->function = func;
                        out->vendor_id = ven;
                        out->device_id = dev;
                        for (int bar = 0; bar < 6; ++bar) {
                            uint32_t value = pci_config_read32(bus, slot, func, 0x10 + bar * 4);
                            out->bar[bar] = value;
                        }
                        uint32_t irq = pci_config_read32(bus, slot, func, 0x3C);
                        out->irq_line = (uint8_t)(irq & 0xFFu);
                    }
                    return 0;
                }
            }
        }
    }
    return -1;
}

void pci_enable_bus_master(const struct pci_device *dev) {
    uint32_t cmd = pci_config_read32(dev->bus, dev->slot, dev->function, 0x04);
    cmd |= 0x00000007u; /* I/O + Memory + Bus Master */
    pci_config_write32(dev->bus, dev->slot, dev->function, 0x04, cmd);
}
