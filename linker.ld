ENTRY(_start)

KERNEL_PHYS_BASE = 0x00100000;
KERNEL_VIRT_BASE = 0xc0000000;

SECTIONS {
    . = KERNEL_PHYS_BASE;

    _kernel_start = .;

    .text.boot ALIGN(4) : {
        *(.text.boot)
    }

    .data.boot ALIGN(4096) : {
        *(.data.boot)
    }

    . += KERNEL_VIRT_BASE;

    .text ALIGN(4) : AT(ADDR(.text) - KERNEL_VIRT_BASE) {
        *(.text.entry)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4) : AT(ADDR(.rodata) - KERNEL_VIRT_BASE) {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4) : AT(ADDR(.data) - KERNEL_VIRT_BASE) {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4) : AT(ADDR(.bss) - KERNEL_VIRT_BASE) {
        _bss_start = .;
        *(COMMON);
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }

    _kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}