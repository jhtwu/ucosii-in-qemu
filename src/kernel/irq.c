#include "ucos_ii.h"
#include "os_core.h"
#include "hw/pic.h"

void irq0_handler_c(void) {
    OSIntEnter();
    OSTimeTick();
    pic_send_eoi(0);
    OSIntExit();
}
