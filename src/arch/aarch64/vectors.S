#define CTX_SIZE 272
#define CTX_SPSR_OFFSET 256
#define CTX_ELR_OFFSET  264

.global __vectors_start
.extern irq_handler
.extern OSTCBCur
.extern OSIntCtxSwPend
.extern OSIntCtxSw

.section .text.boot,"ax"
.align 11
__vectors_start:
    b   sync_el1t
    b   irq_el1t
    b   fiq_el1t
    b   serr_el1t
    b   sync_el1h
    b   irq_el1h
    b   fiq_el1h
    b   serr_el1h
    b   sync_el0_64
    b   irq_el0_64
    b   fiq_el0_64
    b   serr_el0_64
    b   sync_el0_32
    b   irq_el0_32
    b   fiq_el0_32
    b   serr_el0_32

sync_el1t:
irq_el1t:
fiq_el1t:
serr_el1t:
    b   .

sync_el1h:
fiq_el1h:
serr_el1h:
    b   .

sync_el0_64:
irq_el0_64:
fiq_el0_64:
serr_el0_64:
    b   .

sync_el0_32:
irq_el0_32:
fiq_el0_32:
serr_el0_32:
    b   .

irq_el1h:
    /* Save registers to temporary area on SP_EL1 */
    sub     sp, sp, #CTX_SIZE
    stp     x0, x1, [sp, #0]
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    stp     x30, xzr, [sp, #240]
    mrs     x0, spsr_el1
    str     x0, [sp, #CTX_SPSR_OFFSET]
    mrs     x0, elr_el1
    str     x0, [sp, #CTX_ELR_OFFSET]

    /* Allocate space on task stack and copy context */
    mrs     x0, sp_el0
    sub     x0, x0, #CTX_SIZE
    msr     sp_el0, x0
    mov     x1, sp          /* src */
    mov     x2, x0          /* dst */
    mov     x3, #(CTX_SIZE / 16)
1:
    ldp     x4, x5, [x1], #16
    stp     x4, x5, [x2], #16
    subs    x3, x3, #1
    b.ne    1b
    add     sp, sp, #CTX_SIZE

    /* Save SP_EL0 into current TCB */
    adrp    x4, OSTCBCur
    add     x4, x4, :lo12:OSTCBCur
    ldr     x5, [x4]
    str     x0, [x5]

    bl      irq_handler

    /* Check for deferred context switch */
    adrp    x4, OSIntCtxSwPend
    add     x4, x4, :lo12:OSIntCtxSwPend
    ldrb    w5, [x4]
    cbz     w5, 2f
    bl      OSIntCtxSw
2:
    /* Restore context from current task */
    adrp    x4, OSTCBCur
    add     x4, x4, :lo12:OSTCBCur
    ldr     x5, [x4]
    ldr     x6, [x5]
    msr     sp_el0, x6

    mov     x3, x6
    add     x0, x3, #CTX_SIZE
    msr     sp_el0, x0

    ldr     x0, [x3, #CTX_SPSR_OFFSET]
    msr     spsr_el1, x0
    ldr     x0, [x3, #CTX_ELR_OFFSET]
    msr     elr_el1, x0

    ldp     x30, xzr, [x3, #240]
    ldp     x28, x29, [x3, #224]
    ldp     x26, x27, [x3, #208]
    ldp     x24, x25, [x3, #192]
    ldp     x22, x23, [x3, #176]
    ldp     x20, x21, [x3, #160]
    ldp     x18, x19, [x3, #144]
    ldp     x16, x17, [x3, #128]
    ldp     x14, x15, [x3, #112]
    ldp     x12, x13, [x3, #96]
    ldp     x10, x11, [x3, #80]
    ldp     x8, x9, [x3, #64]
    ldp     x6, x7, [x3, #48]
    ldp     x4, x5, [x3, #32]
    ldp     x0, x1, [x3, #0]
    ldp     x2, x3, [x3, #16]
    eret
