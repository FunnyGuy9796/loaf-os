[bits 32]

extern isr_handler

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

isr_common:
    pusha
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    
    call isr_handler

    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8

    iret

ISR_NOERR 32   ; example: timer IRQ (remapped to 0x20)
ISR_NOERR 33   ; example: keyboard IRQ (remapped to 0x21)