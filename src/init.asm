[org 0x7e00]
[bits 16]

%include "kernel_size.inc"

%ifndef KERNEL_SECTORS
%fatal "KERNEL_SECTORS is not defined - build kernel.bin first (run `make`, not `nasm` directly)"
%endif

BOOT_INFO_SEGMENT equ 0x0000
BOOT_INFO_OFFSET equ 0x0500

start:
    mov [bootdrive], dl

    cli

    call check_a20

    cmp ax, 1
    je yes_a20

    call enable_a20
    call check_a20

    cmp ax, 1
    je yes_a20

    jmp $

check_a20:
    pushf
    push ds
    push es
    push di
    push si

    xor ax, ax
    mov es, ax
    mov di, 0x0500

    not ax
    mov ds, ax
    mov si, 0x0510

    mov al, byte [es:di]
    push ax

    mov al, byte [ds:si]
    push ax

    mov byte [es:di], 0x00
    mov byte [ds:si], 0xff

    cmp byte [es:di], 0xff

    pop ax
    mov byte [ds:si], al

    pop ax
    mov byte [es:di], al

    mov ax, 0
    je check_a20_exit

    mov ax, 1

check_a20_exit:
    pop si
    pop di
    pop es
    pop ds
    popf

    ret

a20_wait_input:
    in al, 0x64

    test al, 2
    jnz a20_wait_input

    ret

a20_wait_output:
    in al, 0x64

    test al, 1
    jz a20_wait_output

    ret

enable_a20:
    call a20_wait_input
    mov al, 0xad
    out 0x64, al

    call a20_wait_input
    mov al, 0xd0
    out 0x64, al

    call a20_wait_output
    in al, 0x60
    push ax

    call a20_wait_input
    mov al, 0xd1
    out 0x64, al

    call a20_wait_input
    pop ax
    or al, 2
    out 0x60, al

    call a20_wait_input
    mov al, 0xae
    out 0x64, al

    call a20_wait_input

    ret

yes_a20:
    sti

    clc

    mov ax, BOOT_INFO_SEGMENT
    mov es, ax
    mov di, BOOT_INFO_OFFSET

    int 0x12
    mov [es:di], ax

    mov ah, 0x88

    int 0x15
    mov [es:di+2], ax

    jc mem_error

    xor ax, ax
    mov ds, ax
    mov ax, [ds:0x040e]
    mov [es:di+4], ax

    mov byte [sectors_per_track], 63

    mov si, KERNEL_SECTORS
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x0000
    mov cl, 3
    mov ch, 0
    mov dh, 0

read_loop:
    cmp si, 0
    je kernel_loaded

    mov ah, 0x02
    mov al, 1
    mov dl, [bootdrive]
    int 0x13

    jc read_error

    add bx, 512
    inc cl
    mov al, [sectors_per_track]
    cmp cl, al
    jle no_head_bump
    mov cl, 1
    inc dh
no_head_bump:
    dec si
    jmp read_loop

read_error:
    push ax
    mov ah, 0x0e
    mov al, '!'

    int 0x10
    pop ax
    mov al, ah
    
    call print_hex_byte

    cli
    hlt

kernel_loaded:
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected_entry

mem_error:
    mov ah, 0x0e
    mov al, 'M'

    int 0x10

    mov ah, 0x0e
    mov al, 'E'

    int 0x10

    cli
    hlt

disk_error:
    mov ah, 0x0e
    mov al, '!'

    int 0x10

    mov ah, 0x0e
    mov al, 'R'

    int 0x10

    cli
    hlt

print_hex_byte:
    push ax
    mov bl, al

    mov al, bl
    shr al, 4
    call print_hex_nibble

    mov al, bl
    and al, 0x0f
    call print_hex_nibble

    pop ax
    ret

print_hex_nibble:
    and al, 0x0f
    cmp al, 9
    jle .digit
    add al, 'A' - 10
    jmp .print
.digit:
    add al, '0'
.print:
    mov ah, 0x0e
    int 0x10
    ret

gdt_start:
    dq 0x0000000000000000

gdt_code:
    dw 0xffff
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xffff
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

bootdrive: db 0
sectors_per_track: db 0

[bits 32]

protected_entry:
    cli

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esi, 0x10000
    mov edi, 0x100000
    mov ecx, KERNEL_SECTORS * 512 / 4
    rep movsd

    mov ebx, 0x500

    jmp 0x08:0x100000