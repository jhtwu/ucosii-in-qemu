section .text
    global OSCtxSw
    global OSIntCtxSw
    global OSStartHighRdy

    extern OSTCBCur
    extern OSTCBHighRdy
    extern OSPrioCur
    extern OSPrioHighRdy
    extern OSIntCtxSwPend

OSCtxSw:
    pushfd
    push cs
    push dword [esp + 8]
    pushad
    mov eax, [OSTCBCur]
    mov [eax], esp
    mov eax, [OSTCBHighRdy]
    mov [OSTCBCur], eax
    mov edx, [OSPrioHighRdy]
    mov [OSPrioCur], edx
    mov esp, [eax]
    popad
    iretd

OSIntCtxSw:
    mov byte [OSIntCtxSwPend], 0
    mov eax, [OSTCBCur]
    mov [eax], esp
    mov eax, [OSTCBHighRdy]
    mov [OSTCBCur], eax
    mov edx, [OSPrioHighRdy]
    mov [OSPrioCur], edx
    mov esp, [eax]
    popad
    iretd

OSStartHighRdy:
    mov eax, [OSTCBHighRdy]
    mov [OSTCBCur], eax
    mov edx, [OSPrioHighRdy]
    mov [OSPrioCur], edx
    mov esp, [eax]
    popad
    iretd
