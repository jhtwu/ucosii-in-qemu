#include "os_core.h"
#include "os_cpu.h"
#include "hw/cpu.h"

#include <stdint.h>

#define OS_CPU_SPSR_INIT 0x00000084u
#define CONTEXT_REG_ENTRIES 34u
#define CTX_OFFSET_SPSR 32u
#define CTX_OFFSET_ELR  33u

extern void OSTaskReturn(void);
extern void OSTaskThunk(OS_TASK_PTR task, void *pdata);

OS_STK *OSTaskStkInit(OS_TASK_PTR task, void *pdata, OS_STK *ptos) {
    OS_STK *stk = ptos + 1;
    stk -= CONTEXT_REG_ENTRIES;

    for (uint32_t i = 0; i < CONTEXT_REG_ENTRIES; ++i) {
        stk[i] = 0u;
    }

    stk[0] = (OS_STK)task;          /* x0 */
    stk[1] = (OS_STK)pdata;         /* x1 */
    stk[30] = (OS_STK)OSTaskReturn; /* x30 / LR */
    stk[CTX_OFFSET_SPSR] = OS_CPU_SPSR_INIT;
    stk[CTX_OFFSET_ELR] = (OS_STK)OSTaskThunk;
    return stk;
}

OS_CPU_SR OS_CPU_SaveSR(void) {
    OS_CPU_SR sr;
    __asm__ volatile ("mrs %0, daif" : "=r"(sr));
    cpu_disable_irq();
    return sr;
}

void OS_CPU_RestoreSR(OS_CPU_SR sr) {
    __asm__ volatile ("msr daif, %0" :: "r"(sr) : "memory");
}
