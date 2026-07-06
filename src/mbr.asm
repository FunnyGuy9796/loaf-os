[org 0x7c00]
[bits 16]

jmp short start
nop

oem_name: db "LOAFOS  "     ; 8 bytes
bytes_per_sector: dw 512
sectors_per_cluster: db 1
reserved_sectors: dw 1             ; just this boot sector for now
fat_count: db 2
root_entry_count: dw 224           ; standard for 1.44MB-style layout
total_sectors_16: dw 2880          ; adjust to match your actual image size
media_descriptor: db 0xf8          ; 0xf8 = fixed disk
sectors_per_fat: dw 9
sectors_per_track: dw 63
head_count: dw 16
hidden_sectors: dd 0
total_sectors_32: dd 0
drive_number: db 0x80
reserved1: db 0
boot_signature: db 0x29
volume_id: dd 0x12345678
volume_label: db "LOAFOS     "  ; 11 bytes
fs_type: db "FAT12   "     ; 8 bytes

start:
    cli

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [bootdrive], dl

    sti

    mov ah, 0x02
    mov al, 2
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [bootdrive]
    mov bx, 0x7e00

    int 0x13
    jc disk_error

    mov dl, [bootdrive]
    jmp 0x0000:0x7e00

disk_error:
    mov ah, 0x0e
    mov al, 'D'

    int 0x10

    mov ah, 0x0e
    mov al, 'E'

    int 0x10

    mov ah, 0x0e
    mov al, '1'

    int 0x10

    cli
    hlt

bootdrive: db 0

times 446-($-$$) db 0
times 64 db 0
dw 0xaa55