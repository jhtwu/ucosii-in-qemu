#ifndef HW_PCI_H
#define HW_PCI_H

#include <stdint.h>

struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t bar[6];
    uint8_t irq_line;
};

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);
int pci_find_device(uint16_t vendor, uint16_t device, struct pci_device *out);
void pci_enable_bus_master(const struct pci_device *dev);

#endif /* HW_PCI_H */
