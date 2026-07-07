[bits 32]
global enter_usermode

enter_usermode:
    mov edx, [esp+4]
    mov ebx, [esp+8]
    mov ecx, [esp+12]

    mov cr3, ecx

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    pushfd
    pop eax
    or eax, 0x200

    push dword 0x23
    push ebx
    push eax
    push dword 0x1b
    push edx
    
    iret