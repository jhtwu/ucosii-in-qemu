section .text
    global idt_flush
    global irq0_stub

    extern irq0_handler_c
    extern OSIntCtxSwPend
    extern OSIntCtxSw

idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

irq0_stub:
    pushad
    call irq0_handler_c
    cmp byte [OSIntCtxSwPend], 0
    jne irq0_do_switch
    popad
    iretd

irq0_do_switch:
    jmp OSIntCtxSw
