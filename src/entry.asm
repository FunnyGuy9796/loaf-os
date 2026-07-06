[bits 32]

section .text.entry

global _start
extern kmain

_start:
    mov esp, stack_top

    extern _bss_start
    extern _bss_end
    
    mov edi, _bss_start
    mov ecx, _bss_end

    sub ecx, edi
    xor eax, eax

    rep stosb

    push ebx
    call kmain

    cli
.hang:
    hlt

    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: