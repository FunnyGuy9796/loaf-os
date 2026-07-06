ENTRY(_start)

KERNEL_LOAD_ADDR = 0x100000;

SECTIONS {
    . = KERNEL_LOAD_ADDR;

    _kernel_start = .;

    .text ALIGN(4) : {
        *(.text.entry)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4) : {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4) : {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4) : {
        _bss_start = .;
        *(COMMON);
        *(.bss)
        *(.bss.*)
        _bss_end = .;
    }

    _kernel_end = .;
    _kernel_size = _kernel_end - _kernel_start;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}