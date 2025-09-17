#include "os_core.h"
#include "os_cpu.h"

extern void OSTaskReturn(void);
extern void OSTaskThunk(OS_TASK_PTR task, void *pdata);

OS_STK *OSTaskStkInit(OS_TASK_PTR task, void *pdata, OS_STK *ptos) {
    OS_STK *stk = ptos + 1;

    *(--stk) = (OS_STK)pdata;                 /* Argument for thunk */
    *(--stk) = (OS_STK)task;                  /* Task entry pointer */
    *(--stk) = (OS_STK)OSTaskReturn;          /* Return address after thunk */
    *(--stk) = 0x00000002;                    /* EFLAGS, interrupts off initially */
    *(--stk) = (OS_STK)OS_CPU_GetCS();        /* Current CS selector */
    *(--stk) = (OS_STK)OSTaskThunk;           /* EIP */
    *(--stk) = 0;                             /* EAX */
    *(--stk) = 0;                             /* ECX */
    *(--stk) = 0;                             /* EDX */
    *(--stk) = 0;                             /* EBX */
    *(--stk) = 0;                             /* ESP (dummy) */
    *(--stk) = 0;                             /* EBP */
    *(--stk) = 0;                             /* ESI */
    *(--stk) = 0;                             /* EDI */

    return stk;
}

OS_CPU_SR OS_CPU_SaveSR(void) {
    OS_CPU_SR sr;
    __asm__ volatile ("pushf\n\tpop %0\n\tcli" : "=r"(sr) :: "memory");
    return sr;
}

void OS_CPU_RestoreSR(OS_CPU_SR sr) {
    __asm__ volatile ("push %0\n\tpopf" :: "r"(sr) : "memory", "cc");
}

uint32_t OS_CPU_GetCS(void) {
    uint32_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return cs;
}
