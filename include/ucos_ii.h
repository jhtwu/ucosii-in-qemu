#ifndef UCOS_II_H
#define UCOS_II_H

#include <stdint.h>

typedef uint8_t  INT8U;
typedef uint16_t INT16U;
typedef uint32_t INT32U;
typedef int16_t  INT16S;
typedef int32_t  INT32S;
typedef uint32_t OS_STK;
typedef uint32_t OS_CPU_SR;

typedef void (*OS_TASK_PTR)(void *pdata);

#define OS_MAX_TASKS        8
#define OS_TASK_IDLE_PRIO   (OS_MAX_TASKS - 1)
#define OS_LOWEST_PRIO      OS_TASK_IDLE_PRIO
#define OS_EVENT_NAME_SIZE  16
#define OS_TICKS_PER_SEC    100u

#define OS_ERR_NONE             0u
#define OS_ERR_PRIO_EXIST       40u
#define OS_ERR_PRIO_INVALID     41u
#define OS_ERR_TASK_CREATE      42u

void OSInit(void);
INT8U OSTaskCreate(OS_TASK_PTR task, void *pdata, OS_STK *ptos, INT8U prio);
void OSStart(void);
void OSTimeTick(void);
void OSTimeDly(INT16U ticks);
void OSTimeDlyHMSM(INT8U hours, INT8U minutes, INT8U seconds, INT16U ms);
INT32U OSTimeGet(void);

void OSIntEnter(void);
void OSIntExit(void);

#endif /* UCOS_II_H */
