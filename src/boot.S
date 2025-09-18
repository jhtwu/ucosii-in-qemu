.section .text.boot,"ax"
.align 7
.global _start
.extern __bss_start
.extern __bss_end
.extern __stack_top
.extern __vectors_start
.extern kernel_main

_start:
    /* Ensure we use SP_EL0 for thread mode */
    mov     x0, #0
    msr     SPSel, x0

    /* Set up stacks */
    ldr     x0, =__stack_top
    mov     sp, x0
    sub     x1, x0, #0x4000        /* 16 KiB interrupt stack */
    msr     sp_el1, x1

    /* Zero .bss */
    ldr     x2, =__bss_start
    ldr     x3, =__bss_end
1:
    cmp     x2, x3
    b.ge    2f
    str     xzr, [x2], #8
    b       1b
2:
    /* Install vector table */
    ldr     x4, =__vectors_start
    msr     vbar_el1, x4
    isb

    bl      kernel_main

3:
    wfi
    b       3b
