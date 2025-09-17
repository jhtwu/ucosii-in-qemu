#ifndef OS_CORE_H
#define OS_CORE_H

#include "ucos_ii.h"

typedef struct os_tcb {
    OS_STK *sp;
    OS_TASK_PTR task;
    void *pdata;
    INT16U delay;
    INT8U prio;
    INT8U used;
} OS_TCB;

extern volatile INT8U OSRunning;
extern volatile INT8U OSIntNesting;
extern volatile INT8U OSLockNesting;
extern volatile INT8U OSPrioCur;
extern volatile INT8U OSPrioHighRdy;
extern volatile INT8U OSIntCtxSwPend;
extern OS_TCB *OSTCBCur;
extern OS_TCB *OSTCBHighRdy;

void OSTaskReturn(void);
void OSTaskThunk(OS_TASK_PTR task, void *pdata);
void OSSched(void);
void OSReadyTask(INT8U prio);

#endif /* OS_CORE_H */
