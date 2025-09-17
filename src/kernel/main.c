#include "hw/idt.h"
#include "hw/pic.h"
#include "hw/pit.h"
#include "hw/serial.h"
#include "os_core.h"
#include "os_cpu.h"
#include "ucos_ii.h"
#include <stddef.h>

extern void irq0_stub(void);

static OS_STK TaskAStack[256];
static OS_STK TaskBStack[256];

static void TaskA(void *pdata) {
    (void)pdata;
    INT32U counter = 0;
    for (;;) {
        serial_write("[Task A] tick ");
        serial_write_dec(counter++);
        serial_write("\n");
        OSTimeDlyHMSM(0, 0, 1, 0);
    }
}

static void TaskB(void *pdata) {
    (void)pdata;
    for (;;) {
        serial_write("[Task B] heartbeat\n");
        OSTimeDlyHMSM(0, 0, 0, 250);
    }
}

void kernel_main(void) {
    serial_init();
    serial_write("uC/OS-II x86 demo starting...\n");

    idt_init();
    pic_remap(0x20, 0x28);
    idt_set_gate(32, (uint32_t)irq0_stub, OS_CPU_GetCS(), 0x8E);
    pic_set_mask(0xFFFE);

    pit_init(OS_TICKS_PER_SEC);

    OSInit();
    (void)OSTaskCreate(TaskA, NULL, &TaskAStack[255], 1);
    (void)OSTaskCreate(TaskB, NULL, &TaskBStack[255], 2);

    serial_write("Starting scheduler...\n");
    OSStart();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
