#define CTX_SIZE 272
#define CTX_SPSR_OFFSET 256
#define CTX_ELR_OFFSET  264
#define OS_CPU_SPSR_INIT 0x00000084

.global OSCtxSw
.global OSIntCtxSw
.global OSStartHighRdy

.extern OSTCBCur
.extern OSTCBHighRdy
.extern OSPrioCur
.extern OSPrioHighRdy
.extern OSIntCtxSwPend

/* -------------------------------------------------------------------------- */
/* Context switch from task context                                            */
/* -------------------------------------------------------------------------- */
.text
.align 2
OSCtxSw:
    sub     sp, sp, #CTX_SIZE
    str     x0, [sp, #0]
    str     x1, [sp, #8]
    str     x2, [sp, #16]
    str     x3, [sp, #24]
    str     x4, [sp, #32]
    str     x5, [sp, #40]
    str     x6, [sp, #48]
    str     x7, [sp, #56]
    str     x8, [sp, #64]
    str     x9, [sp, #72]
    str     x10, [sp, #80]
    str     x11, [sp, #88]
    str     x12, [sp, #96]
    str     x13, [sp, #104]
    str     x14, [sp, #112]
    str     x15, [sp, #120]
    str     x16, [sp, #128]
    str     x17, [sp, #136]
    str     x18, [sp, #144]
    str     x19, [sp, #152]
    str     x20, [sp, #160]
    str     x21, [sp, #168]
    str     x22, [sp, #176]
    str     x23, [sp, #184]
    str     x24, [sp, #192]
    str     x25, [sp, #200]
    str     x26, [sp, #208]
    str     x27, [sp, #216]
    str     x28, [sp, #224]
    str     x29, [sp, #232]
    str     x30, [sp, #240]
    mov     x0, #0
    str     x0, [sp, #248]
    mov     x0, #OS_CPU_SPSR_INIT
    str     x0, [sp, #CTX_SPSR_OFFSET]
    str     x30, [sp, #CTX_ELR_OFFSET]

    adrp    x0, OSTCBCur
    add     x0, x0, :lo12:OSTCBCur
    ldr     x1, [x0]
    mov     x8, sp
    str     x8, [x1]

    adrp    x2, OSTCBHighRdy
    add     x2, x2, :lo12:OSTCBHighRdy
    ldr     x3, [x2]
    str     x3, [x0]

    adrp    x4, OSPrioHighRdy
    add     x4, x4, :lo12:OSPrioHighRdy
    ldrb    w5, [x4]
    adrp    x6, OSPrioCur
    add     x6, x6, :lo12:OSPrioCur
    strb    w5, [x6]

    ldr     x7, [x3]
    mov     sp, x7

    ldr     x0, [sp, #CTX_SPSR_OFFSET]
    msr     spsr_el1, x0
    ldr     x0, [sp, #CTX_ELR_OFFSET]
    msr     elr_el1, x0

    ldr     x30, [sp, #240]
    ldr     x29, [sp, #232]
    ldr     x28, [sp, #224]
    ldr     x27, [sp, #216]
    ldr     x26, [sp, #208]
    ldr     x25, [sp, #200]
    ldr     x24, [sp, #192]
    ldr     x23, [sp, #184]
    ldr     x22, [sp, #176]
    ldr     x21, [sp, #168]
    ldr     x20, [sp, #160]
    ldr     x19, [sp, #152]
    ldr     x18, [sp, #144]
    ldr     x17, [sp, #136]
    ldr     x16, [sp, #128]
    ldr     x15, [sp, #120]
    ldr     x14, [sp, #112]
    ldr     x13, [sp, #104]
    ldr     x12, [sp, #96]
    ldr     x11, [sp, #88]
    ldr     x10, [sp, #80]
    ldr     x9, [sp, #72]
    ldr     x8, [sp, #64]
    ldr     x7, [sp, #56]
    ldr     x6, [sp, #48]
    ldr     x5, [sp, #40]
    ldr     x4, [sp, #32]
    ldr     x3, [sp, #24]
    ldr     x2, [sp, #16]
    ldr     x1, [sp, #8]
    ldr     x0, [sp, #0]
    add     sp, sp, #CTX_SIZE
    eret

/* -------------------------------------------------------------------------- */
/* Interrupt context switch                                                   */
/* -------------------------------------------------------------------------- */
.align 2
OSIntCtxSw:
    adrp    x0, OSIntCtxSwPend
    add     x0, x0, :lo12:OSIntCtxSwPend
    mov     w1, #0
    strb    w1, [x0]

    mrs     x1, sp_el0

    adrp    x2, OSTCBCur
    add     x2, x2, :lo12:OSTCBCur
    ldr     x3, [x2]
    str     x1, [x3]

    adrp    x4, OSTCBHighRdy
    add     x4, x4, :lo12:OSTCBHighRdy
    ldr     x5, [x4]
    str     x5, [x2]

    adrp    x6, OSPrioHighRdy
    add     x6, x6, :lo12:OSPrioHighRdy
    ldrb    w7, [x6]
    adrp    x8, OSPrioCur
    add     x8, x8, :lo12:OSPrioCur
    strb    w7, [x8]

    ldr     x9, [x5]
    msr     sp_el0, x9
    ret

/* -------------------------------------------------------------------------- */
/* Start highest ready task                                                   */
/* -------------------------------------------------------------------------- */
.align 2
OSStartHighRdy:
    adrp    x0, OSTCBHighRdy
    add     x0, x0, :lo12:OSTCBHighRdy
    ldr     x1, [x0]

    adrp    x2, OSTCBCur
    add     x2, x2, :lo12:OSTCBCur
    str     x1, [x2]

    adrp    x3, OSPrioHighRdy
    add     x3, x3, :lo12:OSPrioHighRdy
    ldrb    w4, [x3]
    adrp    x5, OSPrioCur
    add     x5, x5, :lo12:OSPrioCur
    strb    w4, [x5]

    ldr     x6, [x1]
    mov     sp, x6

    ldr     x0, [sp, #CTX_SPSR_OFFSET]
    msr     spsr_el1, x0
    ldr     x0, [sp, #CTX_ELR_OFFSET]
    msr     elr_el1, x0

    ldr     x30, [sp, #240]
    ldr     x29, [sp, #232]
    ldr     x28, [sp, #224]
    ldr     x27, [sp, #216]
    ldr     x26, [sp, #208]
    ldr     x25, [sp, #200]
    ldr     x24, [sp, #192]
    ldr     x23, [sp, #184]
    ldr     x22, [sp, #176]
    ldr     x21, [sp, #168]
    ldr     x20, [sp, #160]
    ldr     x19, [sp, #152]
    ldr     x18, [sp, #144]
    ldr     x17, [sp, #136]
    ldr     x16, [sp, #128]
    ldr     x15, [sp, #120]
    ldr     x14, [sp, #112]
    ldr     x13, [sp, #104]
    ldr     x12, [sp, #96]
    ldr     x11, [sp, #88]
    ldr     x10, [sp, #80]
    ldr     x9, [sp, #72]
    ldr     x8, [sp, #64]
    ldr     x7, [sp, #56]
    ldr     x6, [sp, #48]
    ldr     x5, [sp, #40]
    ldr     x4, [sp, #32]
    ldr     x3, [sp, #24]
    ldr     x2, [sp, #16]
    ldr     x1, [sp, #8]
    ldr     x0, [sp, #0]
    add     sp, sp, #CTX_SIZE
    eret
