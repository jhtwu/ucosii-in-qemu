#include "ucos_ii.h"
#include "os_core.h"
#include "os_cpu.h"
#include <stddef.h>

#define OS_IDLE_STK_SIZE 256

static OS_TCB OSTCBTbl[OS_MAX_TASKS];
static OS_TCB *OSTCBPrioTbl[OS_MAX_TASKS];
static OS_STK OSIdleTaskStk[OS_IDLE_STK_SIZE];

volatile INT8U OSRunning = 0;
volatile INT8U OSIntNesting = 0;
volatile INT8U OSLockNesting = 0;
volatile INT8U OSPrioCur = OS_LOWEST_PRIO;
volatile INT8U OSPrioHighRdy = OS_LOWEST_PRIO;
volatile INT8U OSIntCtxSwPend = 0;

static uint32_t OSRdyBits = 0;
static volatile INT32U OSTickCtr = 0;

OS_TCB *OSTCBCur = NULL;
OS_TCB *OSTCBHighRdy = NULL;

extern OS_STK *OSTaskStkInit(OS_TASK_PTR task, void *pdata, OS_STK *ptos);
static void OSIdleTask(void *pdata);
static void OS_SchedNew(void);

void OSTaskReturn(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void OSTaskThunk(OS_TASK_PTR task, void *pdata) {
    __asm__ volatile ("sti");
    task(pdata);
    OSTaskReturn();
}

void OSReadyTask(INT8U prio) {
    OSRdyBits |= (1u << prio);
}

static void OS_RemoveReady(INT8U prio) {
    OSRdyBits &= ~(1u << prio);
}

void OSInit(void) {
    OSRunning = 0;
    OSIntNesting = 0;
    OSLockNesting = 0;
    OSPrioCur = OS_LOWEST_PRIO;
    OSPrioHighRdy = OS_LOWEST_PRIO;
    OSIntCtxSwPend = 0;
    OSRdyBits = 0;
    OSTickCtr = 0;
    OSTCBCur = NULL;
    OSTCBHighRdy = NULL;

    for (int i = 0; i < OS_MAX_TASKS; ++i) {
        OSTCBTbl[i].used = 0;
        OSTCBTbl[i].delay = 0;
        OSTCBTbl[i].prio = (INT8U)i;
        OSTCBTbl[i].sp = NULL;
        OSTCBTbl[i].task = NULL;
        OSTCBTbl[i].pdata = NULL;
        OSTCBPrioTbl[i] = NULL;
    }

    (void)OSTaskCreate(OSIdleTask, NULL, &OSIdleTaskStk[OS_IDLE_STK_SIZE - 1], OS_TASK_IDLE_PRIO);
}

static void OS_SchedNew(void) {
    for (INT8U prio = 0; prio < OS_MAX_TASKS; ++prio) {
        if (OSRdyBits & (1u << prio)) {
            OSPrioHighRdy = prio;
            OSTCBHighRdy = OSTCBPrioTbl[prio];
            return;
        }
    }
    OSPrioHighRdy = OS_TASK_IDLE_PRIO;
    OSTCBHighRdy = OSTCBPrioTbl[OS_TASK_IDLE_PRIO];
}

INT8U OSTaskCreate(OS_TASK_PTR task, void *pdata, OS_STK *ptos, INT8U prio) {
    if (prio >= OS_MAX_TASKS) {
        return OS_ERR_PRIO_INVALID;
    }

    OS_CPU_SR sr = OS_CPU_SaveSR();

    if (OSTCBPrioTbl[prio] != NULL) {
        OS_CPU_RestoreSR(sr);
        return OS_ERR_PRIO_EXIST;
    }

    OS_TCB *tcb = &OSTCBTbl[prio];
    tcb->sp = OSTaskStkInit(task, pdata, ptos);
    tcb->prio = prio;
    tcb->delay = 0;
    tcb->task = task;
    tcb->pdata = pdata;
    tcb->used = 1;
    OSTCBPrioTbl[prio] = tcb;

    OSReadyTask(prio);

    if (OSTCBHighRdy == NULL || prio < OSPrioHighRdy) {
        OSPrioHighRdy = prio;
        OSTCBHighRdy = tcb;
    }

    OS_CPU_RestoreSR(sr);

    if (OSRunning) {
        OSSched();
    }

    return OS_ERR_NONE;
}

void OSStart(void) {
    if (OSRunning) {
        return;
    }
    OSRunning = 1;
    OS_SchedNew();
    OSPrioCur = OSPrioHighRdy;
    OSTCBCur = OSTCBHighRdy;
    OSStartHighRdy();
}

static void OSIdleTask(void *pdata) {
    (void)pdata;
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void OSSched(void) {
    OS_CPU_SR sr = OS_CPU_SaveSR();
    OS_SchedNew();
    if (OSPrioHighRdy != OSPrioCur) {
        if (OSIntNesting == 0u && OSLockNesting == 0u) {
            OS_CPU_RestoreSR(sr);
            OSCtxSw();
        } else {
            OSIntCtxSwPend = 1u;
            OS_CPU_RestoreSR(sr);
        }
    } else {
        OS_CPU_RestoreSR(sr);
    }
}

void OSTimeTick(void) {
    ++OSTickCtr;
    for (INT8U prio = 0; prio < OS_MAX_TASKS; ++prio) {
        OS_TCB *tcb = OSTCBPrioTbl[prio];
        if (tcb == NULL) {
            continue;
        }
        if (tcb->delay > 0) {
            --tcb->delay;
            if (tcb->delay == 0u) {
                OSReadyTask(prio);
            }
        }
    }
}

void OSTimeDly(INT16U ticks) {
    if (ticks == 0u) {
        return;
    }
    OS_CPU_SR sr = OS_CPU_SaveSR();
    OSTCBCur->delay = ticks;
    OS_RemoveReady(OSTCBCur->prio);
    OS_CPU_RestoreSR(sr);
    OSSched();
}

void OSTimeDlyHMSM(INT8U hours, INT8U minutes, INT8U seconds, INT16U ms) {
    uint32_t total_ms = (uint32_t)hours * 3600000u;
    total_ms += (uint32_t)minutes * 60000u;
    total_ms += (uint32_t)seconds * 1000u;
    total_ms += ms;
    uint32_t ticks = (total_ms * OS_TICKS_PER_SEC) / 1000u;
    if (ticks == 0u) {
        ticks = 1u;
    }
    OSTimeDly((INT16U)ticks);
}

INT32U OSTimeGet(void) {
    return OSTickCtr;
}

void OSIntEnter(void) {
    ++OSIntNesting;
}

void OSIntExit(void) {
    if (OSIntNesting == 0u) {
        return;
    }
    --OSIntNesting;
    if (OSIntNesting == 0u) {
        OSSched();
    }
}
