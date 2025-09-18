#include "hw/idt.h"
#include "hw/pic.h"
#include "hw/pit.h"
#include "hw/serial.h"
#include "net/net.h"
#include "os_core.h"
#include "os_cpu.h"
#include "ucos_ii.h"
#include <stddef.h>

extern void irq0_stub(void);

static OS_STK NetTxTaskStack[256];
static OS_STK TaskAStack[256];
static OS_STK TaskBStack[256];
static OS_STK NetTaskStack[256];

static void TaskA(void *pdata) {
    (void)pdata;
    serial_write("[TaskA] start\n");
    for (;;) {
        OSTimeDlyHMSM(0, 0, 1, 0);
    }
}

static void TaskB(void *pdata) {
    (void)pdata;
    serial_write("[TaskB] start\n");
    for (;;) {
        OSTimeDlyHMSM(0, 0, 0, 250);
    }
}

static void NetTask(void *pdata) {
    (void)pdata;
    serial_write("[NetTask] start\n");
    for (;;) {
        net_poll();
        OSTimeDly(1);
    }
}

static void NetTxTask(void *pdata) {
    (void)pdata;
    serial_write("[NetTxTask] start\n");
    const uint8_t host_ip[4] = {192, 168, 1, 103};
    for (;;) {
        serial_write("[NetTxTask] tick\n");
        net_send_arp_probe(host_ip);
        OSTimeDly((INT16U)OS_TICKS_PER_SEC);
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
    net_init();
    const uint8_t host_ip[4] = {192, 168, 1, 103};
    net_send_arp_probe(host_ip);

    OSInit();
    INT8U rc;
    rc = OSTaskCreate(NetTxTask, NULL, &NetTxTaskStack[255], 1);
    if (rc != OS_ERR_NONE) {
        serial_write("[ERR] NetTxTask create failed rc=");
        serial_write_dec(rc);
        serial_write("\n");
    }
    rc = OSTaskCreate(TaskA, NULL, &TaskAStack[255], 2);
    if (rc != OS_ERR_NONE) {
        serial_write("[ERR] TaskA create failed rc=");
        serial_write_dec(rc);
        serial_write("\n");
    }
    rc = OSTaskCreate(TaskB, NULL, &TaskBStack[255], 3);
    if (rc != OS_ERR_NONE) {
        serial_write("[ERR] TaskB create failed rc=");
        serial_write_dec(rc);
        serial_write("\n");
    }
    rc = OSTaskCreate(NetTask, NULL, &NetTaskStack[255], 4);
    if (rc != OS_ERR_NONE) {
        serial_write("[ERR] NetTask create failed rc=");
        serial_write_dec(rc);
        serial_write("\n");
    }

    serial_write("Starting scheduler...\n");
    OSStart();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
