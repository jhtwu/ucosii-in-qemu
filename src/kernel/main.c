#include "hw/cpu.h"
#include "hw/gic.h"
#include "hw/serial.h"
#include "hw/timer.h"
#include "net/net.h"
#include "os_core.h"
#include "os_cpu.h"
#include "ucos_ii.h"

static OS_STK NetTxTaskStack[512];
static OS_STK TaskAStack[512];
static OS_STK TaskBStack[512];
static OS_STK NetTaskStack[512];

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
    serial_write("uC/OS-II arm64 demo starting...\n");

    gic_init();
    timer_init(OS_TICKS_PER_SEC);
    gic_enable_irq(GIC_IRQ_VIRTUAL_TIMER);

    if (net_init() != 0) {
        serial_write("[ERR] net init failed\n");
    }
    const uint8_t host_ip[4] = {192, 168, 1, 103};
    net_send_arp_probe(host_ip);

    OSInit();

    INT8U rc;
    rc = OSTaskCreate(NetTxTask, NULL, &NetTxTaskStack[511], 1);
    if (rc != OS_ERR_NONE) {
        serial_write("[ERR] NetTxTask create failed\n");
    }
    rc = OSTaskCreate(TaskA, NULL, &TaskAStack[511], 2);
    if (rc != OS_ERR_NONE) {
        serial_write("[ERR] TaskA create failed\n");
    }
    rc = OSTaskCreate(TaskB, NULL, &TaskBStack[511], 3);
    if (rc != OS_ERR_NONE) {
        serial_write("[ERR] TaskB create failed\n");
    }
    rc = OSTaskCreate(NetTask, NULL, &NetTaskStack[511], 4);
    if (rc != OS_ERR_NONE) {
        serial_write("[ERR] NetTask create failed\n");
    }

    serial_write("Starting scheduler...\n");
    cpu_enable_irq();
    OSStart();

    for (;;) {
        cpu_wait_for_interrupt();
    }
}
