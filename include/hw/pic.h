#ifndef HW_PIC_H
#define HW_PIC_H

#include <stdint.h>

void pic_remap(int offset1, int offset2);
void pic_set_mask(uint16_t mask);
void pic_send_eoi(uint8_t irq);

#endif /* HW_PIC_H */
