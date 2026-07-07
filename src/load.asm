[org 0x7e00]
[bits 16]

%define SECTOR_SIZE 512
%define RESERVED_SECTORS 8
%define FAT_COUNT 2
%define SECTORS_PER_FAT 9
%define ROOT_ENTRIES 224
%define SECTORS_PER_CLUSTER 1

%define ROOT_START (RESERVED_SECTORS + FAT_COUNT * SECTORS_PER_FAT)
%define ROOT_SECTORS ((ROOT_ENTRIES * 32 + SECTOR_SIZE - 1) / SECTOR_SIZE)
%define DATA_START (ROOT_START + ROOT_SECTORS)

%define ROOT_BUF 0x9000
%define FAT_BUF 0xc000
%define KERNEL_SEG 0x1000

BPB_BASE equ 0x7c00
BPB_RESERVED_SECS equ BPB_BASE + 0x0e
BPB_NUM_FATS equ BPB_BASE + 0x10
BPB_ROOT_ENTRIES equ BPB_BASE + 0x11
BPB_SECTORS_PER_FAT equ BPB_BASE + 0x16

BOOT_INFO_SEGMENT equ 0x0000
BOOT_INFO_OFFSET equ 0x0500

start:
    mov [bootdrive], dl

    cli
    cld

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

    call load_kernel

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

load_kernel:
    call read_bpb

    movzx eax, word [root_start]
    mov cx, [root_sectors]
    xor bx, bx
    mov es, bx
    mov bx, ROOT_BUF
    call read_sectors

    xor bx, bx
    mov es, bx
    mov di, ROOT_BUF
    mov cx, [root_entries]
.scan:
    cmp byte [es:di], 0x00
    je  .fail

    pusha

    mov si, kernel_name
    mov cx, 11

    repe cmpsb

    popa

    je  .found

    add di, 32
    dec cx

    jnz .scan
    jmp .fail

.found:
    mov ax, [es:di + 0x1a]
    mov [cur_cluster], ax

    mov eax, [es:di + 0x1c]
    mov [kernel_size], eax

    movzx eax, word [reserved_sectors]
    mov cx, [sectors_per_fat]
    xor bx, bx
    mov es, bx
    mov bx, FAT_BUF

    call read_sectors

    mov dx, KERNEL_SEG
.chain:
    mov ax, [cur_cluster]

    cmp ax, 0x0ff8
    jae .done

    mov es, dx
    xor bx, bx

    push dx
    sub ax, 2
    movzx bx, byte [sectors_per_cluster]
    mul bx
    add ax, [data_start]
    movzx eax, ax
    movzx cx, byte [sectors_per_cluster]
    
    pop dx

    xor bx, bx
    call read_sectors

    movzx ax, byte [sectors_per_cluster]
    shl ax, 5
    add dx, ax

    call next_cluster

    jmp .chain
.done:
    ret
.fail:
    jmp fat_error

next_cluster:
    push dx

    mov ax, [cur_cluster]
    mov bx, ax
    shr bx, 1
    add bx, ax
    add bx, FAT_BUF
    mov dx, [bx]

    test ax, 1
    jz  .even

    shr dx, 4
    jmp .store
.even:
    and dx, 0x0fff
.store:
    mov [cur_cluster], dx

    pop dx

    ret

read_sectors:
    push dx

    mov [dap_lba], eax
    mov [dap_cnt], cx
    mov [dap_off], bx
    mov [dap_seg], es
    mov ah, 0x42
    mov dl, [bootdrive]
    mov si, dap

    int 0x13

    jc  fat_error

    pop dx

    ret

read_bpb:
    mov ax, [BPB_RESERVED_SECS]
    mov [reserved_sectors], ax
    mov ax, [BPB_SECTORS_PER_FAT]
    mov [sectors_per_fat], ax
    mov ax, [BPB_ROOT_ENTRIES]
    mov [root_entries], ax
    mov al, [BPB_NUM_FATS]
    mov [fat_count], al
    mov al, [BPB_BASE + 0x0d]
    mov [sectors_per_cluster], al

    mov ax, [sectors_per_fat]
    movzx bx, byte [fat_count]
    mul bx
    add ax, [reserved_sectors]
    mov [root_start], ax

    mov ax, [root_entries]
    mov bx, 32
    mul bx
    add ax, SECTOR_SIZE - 1
    adc dx, 0
    mov bx, SECTOR_SIZE
    div bx
    mov [root_sectors], ax

    mov ax, [root_start]
    add ax, [root_sectors]
    mov [data_start], ax

    ret

align 4
dap: db 0x10
        db 0
dap_cnt: dw 0
dap_off: dw 0
dap_seg: dw 0
dap_lba: dd 0
        dd 0
cur_cluster: dw 0
kernel_name: db "KERNEL  BIN"

fat_error:
    mov ah, 0x0e
    mov al, 'F'
    int 0x10
    cli
    hlt

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
kernel_size: dd 0
reserved_sectors: dw 0
sectors_per_fat: dw 0
root_entries: dw 0
fat_count: db 0
root_start: dw 0
root_sectors: dw 0
data_start: dw 0
sectors_per_cluster: db 0

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
    mov ecx, [kernel_size]
    add ecx, 3
    shr ecx, 2
    rep movsd

    mov ebx, 0x500

    jmp 0x08:0x100000