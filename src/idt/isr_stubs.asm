[bits 32]

extern isr_handler
global isr_return

%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr%1
isr%1:
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

isr_return:
    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8
    
    iret

; regular IRQs
ISR_NOERR 32    ; timer IRQ
ISR_NOERR 33    ; keyboard IRQ
ISR_NOERR 128   ; syscall IRQ

; exceptions
ISR_NOERR 0    ; #DE  divide error
ISR_NOERR 1    ; #DB  debug
ISR_NOERR 2    ; NMI
ISR_NOERR 3    ; #BP  breakpoint
ISR_NOERR 4    ; #OF  overflow
ISR_NOERR 5    ; #BR  bound range exceeded
ISR_NOERR 6    ; #UD  invalid opcode
ISR_NOERR 7    ; #NM  device not available
ISR_ERR   8    ; #DF  double fault
ISR_NOERR 9    ; coprocessor segment overrun (legacy)
ISR_ERR   10   ; #TS  invalid TSS
ISR_ERR   11   ; #NP  segment not present
ISR_ERR   12   ; #SS  stack-segment fault
ISR_ERR   13   ; #GP  general protection
ISR_ERR   14   ; #PF  page fault
ISR_NOERR 16   ; #MF  x87 floating point
ISR_ERR   17   ; #AC  alignment check
ISR_NOERR 18   ; #MC  machine check
ISR_NOERR 19   ; #XM  SIMD floating point