#ifndef OS_CPU_H
#define OS_CPU_H

#include <stdint.h>
#include "ucos_ii.h"

void OSCtxSw(void);
void OSIntCtxSw(void);
void OSStartHighRdy(void);
OS_STK *OSTaskStkInit(OS_TASK_PTR task, void *pdata, OS_STK *ptos);
OS_CPU_SR OS_CPU_SaveSR(void);
void OS_CPU_RestoreSR(OS_CPU_SR sr);

#endif /* OS_CPU_H */
