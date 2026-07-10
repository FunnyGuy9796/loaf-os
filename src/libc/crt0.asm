[bits 32]
global _start
extern main
extern sys_exit

section .text
_start:
    mov eax, [esp]
    lea ebx, [esp+4]
    
    push ebx
    push eax

    call main

    add esp, 8

    push eax

    call sys_exit
.hang:
    jmp .hang