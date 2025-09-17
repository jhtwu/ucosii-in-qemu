#ifndef HW_IDT_H
#define HW_IDT_H

#include <stdint.h>

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

#endif /* HW_IDT_H */
