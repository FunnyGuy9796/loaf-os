[bits 32]
global context_switch

context_switch:
    mov eax, [esp+4]
    mov edx, [esp+8]

    push ebx
    push esi
    push edi
    push ebp

    mov [eax], esp
    mov esp, edx

    pop ebp
    pop edi
    pop esi
    pop ebx

    ret