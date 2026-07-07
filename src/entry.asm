[bits 32]

section .text.boot

global _start
extern kmain
extern _bss_start
extern _bss_end
global stack_top

KERNEL_VIRT_BASE equ 0xc0000000
KERNEL_PDE_INDEX equ (KERNEL_VIRT_BASE >> 22)

_start:
    mov edi, boot_page_directory
    mov ecx, 1024
    xor eax, eax
.clear_pd:
    mov [edi], eax
    add edi, 4
    loop .clear_pd

    mov edi, boot_page_table
    mov eax, 0x00000003
    mov ecx, 1024
.fill_pt:
    mov [edi], eax
    add eax, 0x1000
    add edi, 4
    loop .fill_pt

    mov eax, boot_page_table
    or eax, 0x3
    mov [boot_page_directory], eax
    mov [boot_page_directory + KERNEL_PDE_INDEX*4], eax

    mov eax, boot_page_directory
    mov cr3, eax

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    lea eax, [higher_half]
    jmp eax

section .text
higher_half:
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

section .data.boot
align 4096
boot_page_directory: times 1024 dd 0
boot_page_table: times 1024 dd 0

section .bss
align 16
stack_bottom:
    resb 16384
stack_top: